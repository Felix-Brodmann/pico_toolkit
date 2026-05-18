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

/**
 * @class MqttParser
 * @brief Provides static methods for parsing MQTT control packets from byte streams.
 * @details This class contains static methods that take raw byte data and parse it into structured MQTT
 * @ingroup pico_mqtt
 */
class MqttParser
{
private:
    static std::tuple<uint32_t, std::optional<Error>> decode_remaining_length(std::span<const std::byte> data, size_t &offset);

    static bool validate_fixed_header_flags(MqttMessageType type, uint8_t flags);

    static std::tuple<uint16_t, std::optional<Error>> read_u16(std::span<const std::byte> data, size_t &offset);

    static std::tuple<std::string, std::optional<Error>> read_string(std::span<const std::byte> data, size_t &offset);

    static std::tuple<std::span<const std::byte>, std::optional<Error>> read_binary(std::span<const std::byte> data, size_t &offset);

public:
    /**
     * @brief Parses a raw byte stream into an MQTT control packet.
     * @param data The raw byte stream containing the MQTT control packet.
     * @return A tuple containing the parsed MQTT control packet and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttControlPacket, std::optional<Error>> parse_control_packet(std::span<const std::byte> data);

    /**
     * @brief Parses a CONNECT control packet into a structured MqttConnectMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of CONNECT.
     * @return A tuple containing the parsed MqttConnectMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttConnectMessage, std::optional<Error>> parse_connect(const MqttControlPacket &packet);

    /**
     * @brief Parses a CONNACK control packet into a structured MqttConnackMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of CONNACK.
     * @return A tuple containing the parsed MqttConnackMessage and
     */
    [[nodiscard]]
    static std::tuple<MqttConnackMessage, std::optional<Error>> parse_connack(const MqttControlPacket &packet);

    /**
     * @brief Parses a PUBLISH control packet into a structured MqttPublishMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PUBLISH.
     * @return A tuple containing the parsed MqttPublishMessage and an optional
     */
    [[nodiscard]]
    static std::tuple<MqttPublishMessage, std::optional<Error>> parse_publish(const MqttControlPacket &packet);

    /**
     * @brief Parses a PUBACK control packet into a structured MqttPubackMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PUBACK.
     * @return A tuple containing the parsed MqttPubackMessage and an optional error if
     */
    [[nodiscard]]
    static std::tuple<MqttPubackMessage, std::optional<Error>> parse_puback(const MqttControlPacket &packet);

    /**
     * @brief Parses a PUBREC control packet into a structured MqttPubrecMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PUBREC.
     * @return A tuple containing the parsed MqttPubrecMessage and an optional error if
     */
    [[nodiscard]]
    static std::tuple<MqttPubrecMessage, std::optional<Error>> parse_pubrec(const MqttControlPacket &packet);

    /**
     * @brief Parses a PUBREL control packet into a structured MqttPubrelMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PUBREL.
     * @return A tuple containing the parsed MqttPubrelMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttPubrelMessage, std::optional<Error>> parse_pubrel(const MqttControlPacket &packet);

    /**
     * @brief Parses a PUBCOMP control packet into a structured MqttPubcompMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PUBCOMP.
     * @return A tuple containing the parsed MqttPubcompMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttPubcompMessage, std::optional<Error>> parse_pubcomp(const MqttControlPacket &packet);

    /**
     * @brief Parses a SUBSCRIBE control packet into a structured MqttSubscribeMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of SUBSCRIBE.
     * @return A tuple containing the parsed MqttSubscribeMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttSubscribeMessage, std::optional<Error>> parse_subscribe(const MqttControlPacket &packet);

    /**
     * @brief Parses a SUBACK control packet into a structured MqttSubackMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of SUBACK.
     * @return A tuple containing the parsed MqttSubackMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttSubackMessage, std::optional<Error>> parse_suback(const MqttControlPacket &packet);

    /**
     * @brief Parses an UNSUBSCRIBE control packet into a structured MqttUnsubscribeMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of UNSUBSCRIBE.
     * @return A tuple containing the parsed MqttUnsubscribeMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttUnsubscribeMessage, std::optional<Error>> parse_unsubscribe(const MqttControlPacket &packet);

    /**
     * @brief Parses an UNSUBACK control packet into a structured MqttUnsubackMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of UNSUBACK.
     * @return A tuple containing the parsed MqttUnsubackMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttUnsubackMessage, std::optional<Error>> parse_unsuback(const MqttControlPacket &packet);

    /**
     * @brief Parses a PINGREQ control packet into a structured MqttPingreqMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PINGREQ.
     * @return A tuple containing the parsed MqttPingreqMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttPingreqMessage, std::optional<Error>> parse_pingreq(const MqttControlPacket &packet);

    /**
     * @brief Parses a PINGRESP control packet into a structured MqttPingrespMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of PINGRESP.
     * @return A tuple containing the parsed MqttPingrespMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttPingrespMessage, std::optional<Error>> parse_pingresp(const MqttControlPacket &packet);

    /**
     * @brief Parses a DISCONNECT control packet into a structured MqttDisconnectMessage.
     * @param packet The MQTT control packet to parse, which must have a fixed header type of DISCONNECT.
     * @return A tuple containing the parsed MqttDisconnectMessage and an optional error if parsing fails
     */
    [[nodiscard]]
    static std::tuple<MqttDisconnectMessage, std::optional<Error>> parse_disconnect(const MqttControlPacket &packet);
};