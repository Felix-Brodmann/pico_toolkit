/**
 * @file mqtt_encoder.hpp
 * @brief Defines the MqttEncoder class for encoding MQTT messages into byte streams.
 * @author Felix Brodmann
 * @date 2026-05-03
 * @version 1.0.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

#include "error.hpp"
#include "mqtt_message.hpp"

class MqttEncoder
{
private:
    static void write_u8(std::vector<std::byte> &out, uint8_t value);
    static void write_u16(std::vector<std::byte> &out, uint16_t value);
    static void write_string(std::vector<std::byte> &out, const std::string &value);
    static void write_binary(std::vector<std::byte> &out, std::span<const std::byte> value);
    static void write_remaining_length(std::vector<std::byte> &out, uint32_t value);

    static std::tuple<MqttEncodedPacket, std::optional<Error>> make_packet(MqttMessageType type, uint8_t flags, const std::vector<std::byte> &body);

    static std::optional<Error> validate_packet_identifier(uint16_t packet_identifier);

public:
    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_connect(const MqttConnectMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_connack(const MqttConnackMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_publish(const MqttPublishMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_puback(const MqttPubackMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_pubrec(const MqttPubrecMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_pubrel(const MqttPubrelMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_pubcomp(const MqttPubcompMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_subscribe(const MqttSubscribeMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_suback(const MqttSubackMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_unsubscribe(const MqttUnsubscribeMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_unsuback(const MqttUnsubackMessage &msg);

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_pingreq();

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_pingresp();

    [[nodiscard]]
    static std::tuple<MqttEncodedPacket, std::optional<Error>> encode_disconnect();
};