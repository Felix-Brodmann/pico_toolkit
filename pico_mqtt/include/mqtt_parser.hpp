/**
 * @file mqtt_parser.hpp
 * @brief Defines the MqttParser class for parsing MQTT control packets from byte streams.
 * @author Felix Brodmann
 * @date 2026-05-03
 * @version 1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <tuple>

#include "error.hpp"
#include "mqtt_message.hpp"

class MqttParser
{
private:
    static std::tuple<uint32_t, std::optional<Error>> decode_remaining_length(std::span<const std::byte> data, size_t &offset);

    static bool validate_fixed_header_flags(MqttMessageType type, uint8_t flags);

    static std::tuple<uint16_t, std::optional<Error>> read_u16(std::span<const std::byte> data, size_t &offset);

    static std::tuple<std::string, std::optional<Error>> read_string(std::span<const std::byte> data, size_t &offset);

    static std::tuple<std::span<const std::byte>, std::optional<Error>> read_binary(std::span<const std::byte> data, size_t &offset);

public:
    [[nodiscard]]
    static std::tuple<MqttControlPacket, std::optional<Error>> parse_control_packet(std::span<const std::byte> data);

    [[nodiscard]]
    static std::tuple<MqttConnectMessage, std::optional<Error>> parse_connect(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttConnackMessage, std::optional<Error>> parse_connack(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPublishMessage, std::optional<Error>> parse_publish(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPubackMessage, std::optional<Error>> parse_puback(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPubrecMessage, std::optional<Error>> parse_pubrec(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPubrelMessage, std::optional<Error>> parse_pubrel(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPubcompMessage, std::optional<Error>> parse_pubcomp(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttSubscribeMessage, std::optional<Error>> parse_subscribe(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttSubackMessage, std::optional<Error>> parse_suback(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttUnsubscribeMessage, std::optional<Error>> parse_unsubscribe(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttUnsubackMessage, std::optional<Error>> parse_unsuback(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPingreqMessage, std::optional<Error>> parse_pingreq(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttPingrespMessage, std::optional<Error>> parse_pingresp(const MqttControlPacket &packet);

    [[nodiscard]]
    static std::tuple<MqttDisconnectMessage, std::optional<Error>> parse_disconnect(const MqttControlPacket &packet);
};