/**
 * @file mqtt_parser.cpp
 * @brief Implements the MqttParser class for parsing MQTT control packets from byte streams.
 * @author Felix Brodmann
 * @date 2026-05-03
 * @version 1.0.0
 */

#include "mqtt_parser.hpp"

std::tuple<uint32_t, std::optional<Error>> MqttParser::decode_remaining_length(std::span<const std::byte> data, size_t &offset)
{
    uint32_t multiplier = 1;
    out_length          = 0;

    uint8_t encoded_byte = 0;
    uint8_t count        = 0;

    do
    {
        if (offset >= data.size())
        {
            return {0, Error("MQTT remaining length exceeds packet size")};
        }

        encoded_byte = static_cast<uint8_t>(data[offset++]);

        out_length += static_cast<uint32_t>(encoded_byte & 127) * multiplier;
        multiplier *= 128;
        count++;

        if (count > 4)
        {
            return {0, Error("MQTT remaining length exceeds maximum of 4 bytes")};
        }
    } while ((encoded_byte & 128) != 0);

    return {out_length, std::nullopt};
}

bool MqttParser::validate_fixed_header_flags(MqttMessageType type, uint8_t flags)
{
    switch (type)
    {
    case MqttMessageType::PUBLISH:
        return true;

    case MqttMessageType::PUBREL:
    case MqttMessageType::SUBSCRIBE:
    case MqttMessageType::UNSUBSCRIBE:
        return flags == 0b0010;

    default:
        return flags == 0;
    }
}

std::tuple<uint16_t, std::optional<Error>> MqttParser::read_u16(std::span<const std::byte> data, size_t &offset)
{
    if (data.size() - offset < 2)
    {
        return {0, Error("MQTT packet too short while reading uint16")};
    }

    uint16_t value = (static_cast<uint16_t>(data[offset]) << 8) | static_cast<uint16_t>(data[offset + 1]);

    offset += 2;

    return {value, std::nullopt};
}

std::tuple<std::string, std::optional<Error>> MqttParser::read_string(std::span<const std::byte> data, size_t &offset)
{
    auto [len, len_err] = read_u16(data, offset);
    if (len_err)
    {
        return {"", len_err};
    }

    if (data.size() - offset < len)
    {
        return {"", Error("MQTT string length exceeds packet size")};
    }

    std::string value(reinterpret_cast<const char *>(data.data() + offset), len);

    offset += len;

    return {std::move(value), std::nullopt};
}

std::tuple<std::span<const std::byte>, std::optional<Error>> MqttParser::read_binary(std::span<const std::byte> data, size_t &offset)
{
    auto [len, len_err] = read_u16(data, offset);
    if (len_err)
    {
        return {{}, len_err};
    }

    if (data.size() - offset < len)
    {
        return {{}, Error("MQTT binary length exceeds packet size")};
    }

    auto value = data.subspan(offset, len);
    offset += len;

    return {value, std::nullopt};
}

std::tuple<MqttControlPacket, std::optional<Error>> MqttParser::parse_control_packet(std::span<const std::byte> data)
{
    if (data.size() < 2)
    {
        return {{}, Error("MQTT packet too short")};
    }

    size_t  offset = 0;
    uint8_t first  = static_cast<uint8_t>(data[offset++]);
    auto    type   = static_cast<MqttMessageType>(first >> 4);
    uint8_t flags  = first & 0x0F;

    if (type < MqttMessageType::CONNECT || type > MqttMessageType::DISCONNECT)
    {
        return {{}, Error("Invalid MQTT message type")};
    }

    if (!validate_fixed_header_flags(type, flags))
    {
        return {{}, Error("Invalid MQTT fixed header flags")};
    }

    auto [remaining_length, rem_err] = decode_remaining_length(data, offset);
    if (rem_err)
    {
        return {{}, rem_err};
    }

    if (data.size() - offset < remaining_length)
    {
        return {{}, Error("Incomplete MQTT packet")};
    }

    MqttControlPacket packet;
    packet.fixed_header.type             = type;
    packet.fixed_header.flags            = flags;
    packet.fixed_header.remaining_length = remaining_length;
    packet.body                          = data.subspan(offset, remaining_length);

    return {packet, std::nullopt};
}

std::tuple<MqttConnectMessage, std::optional<Error>> MqttParser::parse_connect(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::CONNECT)
    {
        return {{}, Error("MQTT packet is not CONNECT")};
    }

    if (packet.body.size() < 10)
    {
        return {{}, Error("CONNECT packet too short")};
    }

    size_t offset = 0;

    auto [protocol_name, proto_err] = read_string(packet.body, offset);
    if (proto_err)
    {
        return {{}, proto_err};
    }

    if (protocol_name != "MQTT")
    {
        return {{}, Error("Invalid MQTT protocol name")};
    }

    if (packet.body.size() - offset < 1)
    {
        return {{}, Error("Missing MQTT protocol level")};
    }

    uint8_t protocol_level = static_cast<uint8_t>(packet.body[offset++]);
    if (protocol_level != 4)
    {
        return {{}, Error("Unsupported MQTT protocol level")};
    }

    if (packet.body.size() - offset < 1)
    {
        return {{}, Error("Missing CONNECT flags")};
    }

    uint8_t flags = static_cast<uint8_t>(packet.body[offset++]);

    bool    username_flag = (flags & 0b1000'0000) != 0;
    bool    password_flag = (flags & 0b0100'0000) != 0;
    bool    will_retain   = (flags & 0b0010'0000) != 0;
    uint8_t will_qos      = (flags & 0b0001'1000) >> 3;
    bool    will_flag     = (flags & 0b0000'0100) != 0;
    bool    clean_session = (flags & 0b0000'0010) != 0;
    bool    reserved      = (flags & 0b0000'0001) != 0;

    if (reserved)
    {
        return {{}, Error("CONNECT reserved flag must be zero")};
    }

    if (password_flag && !username_flag)
    {
        return {{}, Error("CONNECT password flag requires username flag")};
    }

    if (!will_flag && (will_retain || will_qos != 0))
    {
        return {{}, Error("CONNECT will flags set without will flag")};
    }

    if (will_qos == 3)
    {
        return {{}, Error("CONNECT will QoS must not be 3")};
    }

    auto [keep_alive, keep_err] = read_u16(packet.body, offset);
    if (keep_err)
    {
        return {{}, keep_err};
    }

    MqttConnectMessage msg;
    msg.clean_session = clean_session;
    msg.keep_alive    = keep_alive;

    auto [client_id, client_err] = read_string(packet.body, offset);
    if (client_err)
    {
        return {{}, client_err};
    }

    if (client_id.empty() && !clean_session)
    {
        return {{}, Error("Empty client id requires clean session")};
    }

    msg.client_id = std::move(client_id);

    if (will_flag)
    {
        auto [will_topic, topic_err] = read_string(packet.body, offset);
        if (topic_err)
        {
            return {{}, topic_err};
        }

        auto [will_payload, payload_err] = read_binary(packet.body, offset);
        if (payload_err)
        {
            return {{}, payload_err};
        }

        MqttWillMessage will;
        will.topic   = std::move(will_topic);
        will.message = std::string(reinterpret_cast<const char *>(will_payload.data()), will_payload.size());
        will.qos     = will_qos;
        will.retain  = will_retain;

        msg.will = std::move(will);
    }

    if (username_flag)
    {
        auto [username, username_err] = read_string(packet.body, offset);
        if (username_err)
        {
            return {{}, username_err};
        }

        msg.username = std::move(username);
    }

    if (password_flag)
    {
        auto [password_bytes, password_err] = read_binary(packet.body, offset);
        if (password_err)
        {
            return {{}, password_err};
        }

        msg.password = std::string(reinterpret_cast<const char *>(password_bytes.data()), password_bytes.size());
    }

    if (offset != packet.body.size())
    {
        return {{}, Error("CONNECT packet contains trailing bytes")};
    }

    return {std::move(msg), std::nullopt};
}

std::tuple<MqttConnackMessage, std::optional<Error>> MqttParser::parse_connack(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::CONNACK)
    {
        return {{}, Error("MQTT packet is not CONNACK")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid CONNACK length")};
    }

    uint8_t ack_flags   = static_cast<uint8_t>(packet.body[0]);
    uint8_t return_code = static_cast<uint8_t>(packet.body[1]);

    if ((ack_flags & 0xFE) != 0)
    {
        return {{}, Error("Invalid CONNACK acknowledge flags")};
    }

    if (return_code > 5)
    {
        return {{}, Error("Invalid CONNACK return code")};
    }

    MqttConnackMessage msg;
    msg.session_present = (ack_flags & 0x01) != 0;
    msg.return_code     = static_cast<MqttConnackReturnCode>(return_code);

    return {msg, std::nullopt};
}

std::tuple<MqttPublishMessage, std::optional<Error>> MqttParser::parse_publish(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PUBLISH)
    {
        return {{}, Error("MQTT packet is not PUBLISH")};
    }

    size_t offset = 0;

    auto [topic_name, topic_err] = read_string(packet.body, offset);
    if (topic_err)
    {
        return {{}, topic_err};
    }

    if (topic_name.empty())
    {
        return {{}, Error("PUBLISH topic name must not be empty")};
    }

    uint8_t flags = packet.fixed_header.flags;

    MqttPublishMessage msg;
    msg.dup        = (flags & 0b1000) != 0;
    msg.qos        = (flags & 0b0110) >> 1;
    msg.retain     = (flags & 0b0001) != 0;
    msg.topic_name = std::move(topic_name);

    if (msg.qos == 3)
    {
        return {{}, Error("PUBLISH QoS must not be 3")};
    }

    if (msg.qos > 0)
    {
        auto [packet_id, id_err] = read_u16(packet.body, offset);
        if (id_err)
        {
            return {{}, id_err};
        }

        if (packet_id == 0)
        {
            return {{}, Error("PUBLISH packet identifier must not be zero")};
        }

        msg.packet_identifier = packet_id;
    }

    msg.payload = packet.body.subspan(offset);

    return {std::move(msg), std::nullopt};
}

std::tuple<MqttPubackMessage, std::optional<Error>> MqttParser::parse_puback(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PUBACK)
    {
        return {{}, Error("MQTT packet is not PUBACK")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid PUBACK length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("PUBACK packet identifier must not be zero")};
    }

    MqttPubackMessage msg;
    msg.packet_identifier = packet_id;

    return {msg, std::nullopt};
}

std::tuple<MqttPubrecMessage, std::optional<Error>> MqttParser::parse_pubrec(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PUBREC)
    {
        return {{}, Error("MQTT packet is not PUBREC")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid PUBREC length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("PUBREC packet identifier must not be zero")};
    }

    MqttPubrecMessage msg;
    msg.packet_identifier = packet_id;

    return {msg, std::nullopt};
}

std::tuple<MqttPubrelMessage, std::optional<Error>> MqttParser::parse_pubrel(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PUBREL)
    {
        return {{}, Error("MQTT packet is not PUBREL")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid PUBREL length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("PUBREL packet identifier must not be zero")};
    }

    MqttPubrelMessage msg;
    msg.packet_identifier = packet_id;

    return {msg, std::nullopt};
}

std::tuple<MqttPubcompMessage, std::optional<Error>> MqttParser::parse_pubcomp(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PUBCOMP)
    {
        return {{}, Error("MQTT packet is not PUBCOMP")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid PUBCOMP length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("PUBCOMP packet identifier must not be zero")};
    }

    MqttPubcompMessage msg;
    msg.packet_identifier = packet_id;

    return {msg, std::nullopt};
}

std::tuple<MqttSubscribeMessage, std::optional<Error>> MqttParser::parse_subscribe(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::SUBSCRIBE)
    {
        return {{}, Error("MQTT packet is not SUBSCRIBE")};
    }

    if (packet.body.size() < 5)
    {
        return {{}, Error("Invalid SUBSCRIBE length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("SUBSCRIBE packet identifier must not be zero")};
    }

    MqttSubscribeMessage msg;
    msg.packet_identifier = packet_id;

    while (offset < packet.body.size())
    {
        auto [topic_filter, topic_err] = read_string(packet.body, offset);
        if (topic_err)
        {
            return {{}, topic_err};
        }

        if (topic_filter.empty())
        {
            return {{}, Error("SUBSCRIBE topic filter must not be empty")};
        }

        if (offset >= packet.body.size())
        {
            return {{}, Error("SUBSCRIBE missing requested QoS")};
        }

        uint8_t qos = static_cast<uint8_t>(packet.body[offset++]);

        if ((qos & 0b1111'1100) != 0)
        {
            return {{}, Error("SUBSCRIBE requested QoS reserved bits must be zero")};
        }

        qos &= 0b0000'0011;

        if (qos > 2)
        {
            return {{}, Error("SUBSCRIBE requested QoS must be 0, 1, or 2")};
        }

        MqttSubscribeTopic topic;
        topic.topic_filter = std::move(topic_filter);
        topic.qos          = qos;

        msg.topics.push_back(std::move(topic));
    }

    if (msg.topics.empty())
    {
        return {{}, Error("SUBSCRIBE must contain at least one topic filter")};
    }

    return {std::move(msg), std::nullopt};
}

std::tuple<MqttSubackMessage, std::optional<Error>> MqttParser::parse_suback(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::SUBACK)
    {
        return {{}, Error("MQTT packet is not SUBACK")};
    }

    if (packet.body.size() < 3)
    {
        return {{}, Error("Invalid SUBACK length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("SUBACK packet identifier must not be zero")};
    }

    MqttSubackMessage msg;
    msg.packet_identifier = packet_id;

    while (offset < packet.body.size())
    {
        uint8_t code = static_cast<uint8_t>(packet.body[offset++]);

        if (code != 0x00 && code != 0x01 && code != 0x02 && code != 0x80)
        {
            return {{}, Error("Invalid SUBACK return code")};
        }

        msg.return_codes.push_back(static_cast<MqttSubackReturnCode>(code));
    }

    if (msg.return_codes.empty())
    {
        return {{}, Error("SUBACK must contain at least one return code")};
    }

    return {std::move(msg), std::nullopt};
}

std::tuple<MqttUnsubscribeMessage, std::optional<Error>> MqttParser::parse_unsubscribe(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::UNSUBSCRIBE)
    {
        return {{}, Error("MQTT packet is not UNSUBSCRIBE")};
    }

    if (packet.body.size() < 4)
    {
        return {{}, Error("Invalid UNSUBSCRIBE length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("UNSUBSCRIBE packet identifier must not be zero")};
    }

    MqttUnsubscribeMessage msg;
    msg.packet_identifier = packet_id;

    while (offset < packet.body.size())
    {
        auto [topic_filter, topic_err] = read_string(packet.body, offset);
        if (topic_err)
        {
            return {{}, topic_err};
        }

        if (topic_filter.empty())
        {
            return {{}, Error("UNSUBSCRIBE topic filter must not be empty")};
        }

        msg.topic_filters.push_back(std::move(topic_filter));
    }

    if (msg.topic_filters.empty())
    {
        return {{}, Error("UNSUBSCRIBE must contain at least one topic filter")};
    }

    return {std::move(msg), std::nullopt};
}

std::tuple<MqttUnsubackMessage, std::optional<Error>> MqttParser::parse_unsuback(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::UNSUBACK)
    {
        return {{}, Error("MQTT packet is not UNSUBACK")};
    }

    if (packet.body.size() != 2)
    {
        return {{}, Error("Invalid UNSUBACK length")};
    }

    size_t offset         = 0;
    auto [packet_id, err] = read_u16(packet.body, offset);
    if (err)
    {
        return {{}, err};
    }

    if (packet_id == 0)
    {
        return {{}, Error("UNSUBACK packet identifier must not be zero")};
    }

    MqttUnsubackMessage msg;
    msg.packet_identifier = packet_id;

    return {msg, std::nullopt};
}

std::tuple<MqttPingreqMessage, std::optional<Error>> MqttParser::parse_pingreq(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PINGREQ)
    {
        return {{}, Error("MQTT packet is not PINGREQ")};
    }

    if (!packet.body.empty())
    {
        return {{}, Error("PINGREQ body must be empty")};
    }

    return {MqttPingreqMessage{}, std::nullopt};
}

std::tuple<MqttPingrespMessage, std::optional<Error>> MqttParser::parse_pingresp(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::PINGRESP)
    {
        return {{}, Error("MQTT packet is not PINGRESP")};
    }

    if (!packet.body.empty())
    {
        return {{}, Error("PINGRESP body must be empty")};
    }

    return {MqttPingrespMessage{}, std::nullopt};
}

std::tuple<MqttDisconnectMessage, std::optional<Error>> MqttParser::parse_disconnect(const MqttControlPacket &packet)
{
    if (packet.fixed_header.type != MqttMessageType::DISCONNECT)
    {
        return {{}, Error("MQTT packet is not DISCONNECT")};
    }

    if (!packet.body.empty())
    {
        return {{}, Error("DISCONNECT body must be empty")};
    }

    return {MqttDisconnectMessage{}, std::nullopt};
}