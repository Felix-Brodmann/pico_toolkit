#include "mqtt_client.hpp"

uint16_t MqttClient::allocate_packet_identifier()
{
    uint16_t id = next_packet_identifier++;

    if (next_packet_identifier == 0)
    {
        next_packet_identifier = 1;
    }

    return id;
}

std::optional<Error> MqttClient::send_encoded(const MqttEncodedPacket &packet)
{
    if (!conn)
    {
        return Error("MQTT client is not connected");
    }

    auto [written, err] = conn->write(packet.bytes.data(), packet.bytes.size());
    if (err)
    {
        state = MqttClientState::ERROR;
        return err;
    }

    if (written != static_cast<int>(packet.bytes.size()))
    {
        state = MqttClientState::ERROR;
        return Error("MQTT packet was only partially written");
    }

    last_tx = get_absolute_time();

    return std::nullopt;
}

std::optional<Error> MqttClient::connect(const Address &broker, const MqttConnectMessage &connect_message, uint32_t timeout_ms)
{
    if (state == MqttClientState::CONNECTED)
    {
        return Error("MQTT client already connected");
    }

    auto [tcp_conn, dial_err] = network.dial_tcp(broker, timeout_ms);
    if (dial_err)
    {
        state = MqttClientState::ERROR;
        return dial_err;
    }

    conn               = std::move(tcp_conn);
    keep_alive_seconds = connect_message.keep_alive;

    auto [connect_packet, encode_err] = MqttEncoder::encode_connect(connect_message);
    if (encode_err)
    {
        state = MqttClientState::ERROR;
        return encode_err;
    }

    auto send_err = send_encoded(connect_packet);
    if (send_err)
    {
        return send_err;
    }

    std::byte buffer[256];

    absolute_time_t until = make_timeout_time_ms(timeout_ms);

    while (true)
    {
        auto [n, read_err] = conn->read_nonblocking(buffer, sizeof(buffer));
        if (read_err)
        {
            state = MqttClientState::ERROR;
            return read_err;
        }

        if (n > 0)
        {
            last_rx = get_absolute_time();

            auto packet_span          = std::span<const std::byte>(buffer, static_cast<size_t>(n));
            auto [packet, packet_err] = MqttParser::parse_control_packet(packet_span);
            if (packet_err)
            {
                state = MqttClientState::ERROR;
                return packet_err;
            }

            auto [connack, connack_err] = MqttParser::parse_connack(packet);
            if (connack_err)
            {
                state = MqttClientState::ERROR;
                return connack_err;
            }

            if (connack.return_code != MqttConnackReturnCode::ACCEPTED)
            {
                state = MqttClientState::ERROR;
                return Error("MQTT broker rejected connection");
            }

            state = MqttClientState::CONNECTED;
            return std::nullopt;
        }

        if (absolute_time_diff_us(get_absolute_time(), until) <= 0)
        {
            state = MqttClientState::ERROR;
            return Error("MQTT CONNACK timeout");
        }

        sleep_ms(10);
    }
}

std::optional<Error> MqttClient::publish(const std::string &topic, std::span<const std::byte> payload, bool retain)
{
    if (state != MqttClientState::CONNECTED)
    {
        return Error("MQTT client is not connected");
    }

    MqttPublishMessage msg;
    msg.topic_name = topic;
    msg.payload    = payload;
    msg.qos        = 0;
    msg.retain     = retain;

    auto [packet, encode_err] = MqttEncoder::encode_publish(msg);
    if (encode_err)
    {
        return encode_err;
    }

    return send_encoded(packet);
}

std::optional<Error> MqttClient::publish(const std::string &topic, const std::string &payload, bool retain)
{
    auto span = std::span<const std::byte>(reinterpret_cast<const std::byte *>(payload.data()), payload.size());

    return publish(topic, span, retain);
}

std::optional<Error> MqttClient::subscribe(const std::string &topic, uint8_t qos)
{
    if (state != MqttClientState::CONNECTED)
    {
        return Error("MQTT client is not connected");
    }

    if (qos > 0)
    {
        return Error("MQTT client currently supports subscribe QoS 0 only");
    }

    MqttSubscribeMessage msg;
    msg.packet_identifier = allocate_packet_identifier();

    MqttSubscribeTopic sub_topic;
    sub_topic.topic_filter = topic;
    sub_topic.qos          = qos;

    msg.topics.push_back(std::move(sub_topic));

    auto [packet, encode_err] = MqttEncoder::encode_subscribe(msg);
    if (encode_err)
    {
        return encode_err;
    }

    return send_encoded(packet);
}

std::optional<Error> MqttClient::handle_packet(const MqttControlPacket &packet)
{
    switch (packet.fixed_header.type)
    {
    case MqttMessageType::PUBLISH:
    {
        auto [msg, err] = MqttParser::parse_publish(packet);
        if (err)
        {
            return err;
        }

        if (msg.qos > 0)
        {
            return Error("MQTT client currently supports incoming QoS 0 only");
        }

        incoming_messages.push_back(std::move(msg));
        return std::nullopt;
    }

    case MqttMessageType::PINGRESP:
    {
        auto [msg, err] = MqttParser::parse_pingresp(packet);
        if (err)
        {
            return err;
        }

        waiting_for_pingresp = false;
        return std::nullopt;
    }

    case MqttMessageType::SUBACK:
    {
        auto [msg, err] = MqttParser::parse_suback(packet);
        if (err)
        {
            return err;
        }

        return std::nullopt;
    }

    default:
        return std::nullopt;
    }
}

std::optional<Error> MqttClient::poll()
{
    if (state != MqttClientState::CONNECTED)
    {
        return Error("MQTT client is not connected");
    }

    std::byte buffer[256];

    auto [n, read_err] = conn->read_nonblocking(buffer, sizeof(buffer));
    if (read_err)
    {
        state = MqttClientState::ERROR;
        return read_err;
    }

    if (n > 0)
    {
        last_rx = get_absolute_time();

        auto packet_span          = std::span<const std::byte>(buffer, static_cast<size_t>(n));
        auto [packet, packet_err] = MqttParser::parse_control_packet(packet_span);
        if (packet_err)
        {
            state = MqttClientState::ERROR;
            return packet_err;
        }

        auto handle_err = handle_packet(packet);
        if (handle_err)
        {
            state = MqttClientState::ERROR;
            return handle_err;
        }
    }

    if (keep_alive_seconds > 0)
    {
        int64_t since_last_tx_us = absolute_time_diff_us(last_tx, get_absolute_time());

        int64_t keep_alive_us = static_cast<int64_t>(keep_alive_seconds) * 1000 * 1000;

        if (!waiting_for_pingresp && since_last_tx_us >= keep_alive_us)
        {
            auto [packet, err] = MqttEncoder::encode_pingreq();
            if (err)
            {
                return err;
            }

            auto send_err = send_encoded(packet);
            if (send_err)
            {
                return send_err;
            }

            waiting_for_pingresp = true;
        }

        if (waiting_for_pingresp)
        {
            int64_t since_last_rx_us = absolute_time_diff_us(last_rx, get_absolute_time());

            if (since_last_rx_us >= keep_alive_us + keep_alive_us / 2)
            {
                state = MqttClientState::ERROR;
                return Error("MQTT PINGRESP timeout");
            }
        }
    }

    return std::nullopt;
}

bool MqttClient::has_message() const
{
    return !incoming_messages.empty();
}

std::tuple<MqttPublishMessage, std::optional<Error>> MqttClient::next_message()
{
    if (incoming_messages.empty())
    {
        return {{}, Error("No MQTT message available")};
    }

    MqttPublishMessage msg = std::move(incoming_messages.front());
    incoming_messages.pop_front();

    return {std::move(msg), std::nullopt};
}

std::optional<Error> MqttClient::disconnect()
{
    if (state != MqttClientState::CONNECTED)
    {
        return std::nullopt;
    }

    auto [packet, encode_err] = MqttEncoder::encode_disconnect();
    if (encode_err)
    {
        return encode_err;
    }

    auto send_err = send_encoded(packet);

    if (conn)
    {
        conn->close();
        conn.reset();
    }

    state = MqttClientState::DISCONNECTED;

    return send_err;
}

bool MqttClient::is_connected() const
{
    return state == MqttClientState::CONNECTED;
}