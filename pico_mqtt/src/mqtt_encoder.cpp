/**
 * @file mqtt_encoder.cpp
 * @brief Implements the MqttEncoder class for encoding MQTT messages into byte streams.
 * @author Felix Brodmann
 * @date 2026-05-03
 * @version 1.0.0
 */

#include "mqtt_encoder.hpp"

void MqttEncoder::write_u8(std::vector<std::byte> &out, uint8_t value)
{
    out.push_back(static_cast<std::byte>(value));
}

void MqttEncoder::write_u16(std::vector<std::byte> &out, uint16_t value)
{
    out.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::byte>(value & 0xFF));
}

void MqttEncoder::write_string(std::vector<std::byte> &out, const std::string &value)
{
    write_u16(out, static_cast<uint16_t>(value.size()));

    out.insert(out.end(), reinterpret_cast<const std::byte *>(value.data()), reinterpret_cast<const std::byte *>(value.data() + value.size()));
}

void MqttEncoder::write_binary(std::vector<std::byte> &out, std::span<const std::byte> value)
{
    write_u16(out, static_cast<uint16_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

void MqttEncoder::write_remaining_length(std::vector<std::byte> &out, uint32_t value)
{
    do
    {
        uint8_t encoded_byte = value % 128;
        value /= 128;

        if (value > 0)
        {
            encoded_byte |= 128;
        }

        write_u8(out, encoded_byte);
    } while (value > 0);
}

std::optional<Error> MqttEncoder::validate_packet_identifier(uint16_t packet_identifier)
{
    if (packet_identifier == 0)
    {
        return Error("MQTT packet identifier must not be zero");
    }

    return std::nullopt;
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::make_packet(MqttMessageType type, uint8_t flags, const std::vector<std::byte> &body)
{
    if (body.size() > 268435455UL)
    {
        return {{}, Error("MQTT packet too large")};
    }

    MqttEncodedPacket packet;

    uint8_t first_byte = static_cast<uint8_t>((static_cast<uint8_t>(type) << 4) | (flags & 0x0F));

    write_u8(packet.bytes, first_byte);
    write_remaining_length(packet.bytes, static_cast<uint32_t>(body.size()));

    packet.bytes.insert(packet.bytes.end(), body.begin(), body.end());

    return {std::move(packet), std::nullopt};
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_connect(const MqttConnectMessage &msg)
{
    if (msg.client_id.empty() && !msg.clean_session)
    {
        return {{}, Error("MQTT CONNECT with empty client_id requires clean_session=true")};
    }

    if (msg.will && msg.will->qos > 2)
    {
        return {{}, Error("MQTT will QoS must be 0, 1, or 2")};
    }

    if (msg.password && !msg.username)
    {
        return {{}, Error("MQTT password requires username")};
    }

    std::vector<std::byte> body;

    // Variable header
    write_string(body, "MQTT");
    write_u8(body, 0x04);

    uint8_t connect_flags = 0;

    if (msg.username)
    {
        connect_flags |= 0b1000'0000;
    }

    if (msg.password)
    {
        connect_flags |= 0b0100'0000;
    }

    if (msg.will)
    {
        connect_flags |= 0b0000'0100;
        connect_flags |= static_cast<uint8_t>((msg.will->qos & 0b11) << 3);

        if (msg.will->retain)
        {
            connect_flags |= 0b0010'0000;
        }
    }

    if (msg.clean_session)
    {
        connect_flags |= 0b0000'0010;
    }

    write_u8(body, connect_flags);
    write_u16(body, msg.keep_alive);

    // Payload
    write_string(body, msg.client_id);

    if (msg.will)
    {
        write_string(body, msg.will->topic);

        auto will_payload = std::span<const std::byte>(reinterpret_cast<const std::byte *>(msg.will->message.data()), msg.will->message.size());

        write_binary(body, will_payload);
    }

    if (msg.username)
    {
        write_string(body, *msg.username);
    }

    if (msg.password)
    {
        auto password_payload = std::span<const std::byte>(reinterpret_cast<const std::byte *>(msg.password->data()), msg.password->size());

        write_binary(body, password_payload);
    }

    return make_packet(MqttMessageType::CONNECT, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_connack(const MqttConnackMessage &msg)
{
    uint8_t return_code = static_cast<uint8_t>(msg.return_code);

    if (return_code > 5)
    {
        return {{}, Error("MQTT CONNACK invalid return code")};
    }

    std::vector<std::byte> body;

    uint8_t ack_flags = msg.session_present ? 0x01 : 0x00;

    write_u8(body, ack_flags);
    write_u8(body, return_code);

    return make_packet(MqttMessageType::CONNACK, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_publish(const MqttPublishMessage &msg)
{
    if (msg.topic_name.empty())
    {
        return {{}, Error("MQTT PUBLISH topic name must not be empty")};
    }

    if (msg.qos > 2)
    {
        return {{}, Error("MQTT PUBLISH QoS must be 0, 1, or 2")};
    }

    if (msg.qos == 0 && msg.packet_identifier)
    {
        return {{}, Error("MQTT PUBLISH QoS 0 must not include packet identifier")};
    }

    if (msg.qos > 0 && !msg.packet_identifier)
    {
        return {{}, Error("MQTT PUBLISH QoS > 0 requires packet identifier")};
    }

    if (msg.packet_identifier && *msg.packet_identifier == 0)
    {
        return {{}, Error("MQTT PUBLISH packet identifier must not be zero")};
    }

    std::vector<std::byte> body;

    write_string(body, msg.topic_name);

    if (msg.qos > 0)
    {
        write_u16(body, *msg.packet_identifier);
    }

    body.insert(body.end(), msg.payload.begin(), msg.payload.end());

    uint8_t flags = 0;

    if (msg.dup)
    {
        flags |= 0b1000;
    }

    flags |= static_cast<uint8_t>((msg.qos & 0b11) << 1);

    if (msg.retain)
    {
        flags |= 0b0001;
    }

    return make_packet(MqttMessageType::PUBLISH, flags, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_puback(const MqttPubackMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    std::vector<std::byte> body;
    write_u16(body, msg.packet_identifier);

    return make_packet(MqttMessageType::PUBACK, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_pubrec(const MqttPubrecMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    std::vector<std::byte> body;
    write_u16(body, msg.packet_identifier);

    return make_packet(MqttMessageType::PUBREC, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_pubrel(const MqttPubrelMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    std::vector<std::byte> body;
    write_u16(body, msg.packet_identifier);

    return make_packet(MqttMessageType::PUBREL, 0b0010, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_pubcomp(const MqttPubcompMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    std::vector<std::byte> body;
    write_u16(body, msg.packet_identifier);

    return make_packet(MqttMessageType::PUBCOMP, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_subscribe(const MqttSubscribeMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    if (msg.topics.empty())
    {
        return {{}, Error("MQTT SUBSCRIBE must contain at least one topic")};
    }

    std::vector<std::byte> body;

    write_u16(body, msg.packet_identifier);

    for (const auto &topic : msg.topics)
    {
        if (topic.topic_filter.empty())
        {
            return {{}, Error("MQTT SUBSCRIBE topic filter must not be empty")};
        }

        if (topic.qos > 2)
        {
            return {{}, Error("MQTT SUBSCRIBE QoS must be 0, 1, or 2")};
        }

        write_string(body, topic.topic_filter);
        write_u8(body, topic.qos);
    }

    return make_packet(MqttMessageType::SUBSCRIBE, 0b0010, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_suback(const MqttSubackMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    if (msg.return_codes.empty())
    {
        return {{}, Error("MQTT SUBACK must contain at least one return code")};
    }

    std::vector<std::byte> body;

    write_u16(body, msg.packet_identifier);

    for (const auto &code : msg.return_codes)
    {
        uint8_t raw = static_cast<uint8_t>(code);

        if (raw != 0x00 && raw != 0x01 && raw != 0x02 && raw != 0x80)
        {
            return {{}, Error("MQTT SUBACK invalid return code")};
        }

        write_u8(body, raw);
    }

    return make_packet(MqttMessageType::SUBACK, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_unsubscribe(const MqttUnsubscribeMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    if (msg.topic_filters.empty())
    {
        return {{}, Error("MQTT UNSUBSCRIBE must contain at least one topic filter")};
    }

    std::vector<std::byte> body;

    write_u16(body, msg.packet_identifier);

    for (const auto &topic_filter : msg.topic_filters)
    {
        if (topic_filter.empty())
        {
            return {{}, Error("MQTT UNSUBSCRIBE topic filter must not be empty")};
        }

        write_string(body, topic_filter);
    }

    return make_packet(MqttMessageType::UNSUBSCRIBE, 0b0010, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_unsuback(const MqttUnsubackMessage &msg)
{
    if (auto err = validate_packet_identifier(msg.packet_identifier))
    {
        return {{}, err};
    }

    std::vector<std::byte> body;

    write_u16(body, msg.packet_identifier);

    return make_packet(MqttMessageType::UNSUBACK, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_pingreq()
{
    std::vector<std::byte> body;
    return make_packet(MqttMessageType::PINGREQ, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_pingresp()
{
    std::vector<std::byte> body;
    return make_packet(MqttMessageType::PINGRESP, 0x00, body);
}

std::tuple<MqttEncodedPacket, std::optional<Error>> MqttEncoder::encode_disconnect()
{
    std::vector<std::byte> body;
    return make_packet(MqttMessageType::DISCONNECT, 0x00, body);
}