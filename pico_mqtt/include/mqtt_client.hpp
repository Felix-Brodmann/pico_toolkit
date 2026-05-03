/**
 * @file mqtt_client.hpp
 * @brief Defines the MqttClient class for managing MQTT connections and messaging.
 * @author Felix Brodmann
 * @date 2026-05-03
 * @version 1.0.0
 */

#pragma once

#include <cstring>
#include <deque>
#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "address.hpp"
#include "connection.hpp"
#include "error.hpp"
#include "network.hpp"

#include "mqtt_encoder.hpp"
#include "mqtt_message.hpp"
#include "mqtt_parser.hpp"

enum class MqttClientState
{
    DISCONNECTED,
    CONNECTED,
    ERROR
};

class MqttClient
{
private:
    Network                    &network;
    std::unique_ptr<Connection> conn;

    MqttClientState state = MqttClientState::DISCONNECTED;

    uint16_t next_packet_identifier = 1;
    uint16_t keep_alive_seconds     = 60;

    absolute_time_t last_tx = nil_time;
    absolute_time_t last_rx = nil_time;

    bool waiting_for_pingresp = false;

    std::vector<std::byte>         rx_buffer;
    std::deque<MqttPublishMessage> incoming_messages;

    uint16_t allocate_packet_identifier();

    std::optional<Error> send_encoded(const MqttEncodedPacket &packet);
    std::optional<Error> handle_packet(const MqttControlPacket &packet);

public:
    explicit MqttClient(Network &network) : network(network) {}

    [[nodiscard]]
    std::optional<Error> connect(const Address &broker, const MqttConnectMessage &connect_message, uint32_t timeout_ms = 10000);

    [[nodiscard]]
    std::optional<Error> publish(const std::string &topic, std::span<const std::byte> payload, bool retain = false);

    [[nodiscard]]
    std::optional<Error> publish(const std::string &topic, const std::string &payload, bool retain = false);

    [[nodiscard]]
    std::optional<Error> subscribe(const std::string &topic, uint8_t qos = 0);

    [[nodiscard]]
    std::optional<Error> poll();

    bool has_message() const;

    [[nodiscard]]
    std::tuple<MqttPublishMessage, std::optional<Error>> next_message();

    [[nodiscard]]
    std::optional<Error> disconnect();

    bool is_connected() const;
};