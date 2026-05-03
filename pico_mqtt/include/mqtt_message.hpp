/**
 * @file mqtt_message.hpp
 * @brief Defines the MQTT message structures for representing MQTT control packets and their contents.
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
#include <vector>

enum class MqttMessageType : uint8_t
{
    CONNECT     = 0x1,
    CONNACK     = 0x2,
    PUBLISH     = 0x3,
    PUBACK      = 0x4,
    PUBREC      = 0x5,
    PUBREL      = 0x6,
    PUBCOMP     = 0x7,
    SUBSCRIBE   = 0x8,
    SUBACK      = 0x9,
    UNSUBSCRIBE = 0xA,
    UNSUBACK    = 0xB,
    PINGREQ     = 0xC,
    PINGRESP    = 0xD,
    DISCONNECT  = 0xE
};

struct MqttFixedHeader
{
    MqttMessageType type;
    uint8_t         flags            = 0;
    uint32_t        remaining_length = 0;
};

struct MqttControlPacket
{
    MqttFixedHeader fixed_header;

    // The body of the control packet, which may contain variable header and payload.
    std::span<const std::byte> body;
};

struct MqttEncodedPacket
{
    std::vector<std::byte> bytes;
};

/**********************
 *****Mqtt Connect*****
 **********************/

struct MqttWillMessage
{
    std::string topic;
    std::string message;
    uint8_t     qos    = 0;
    bool        retain = false;
};

struct MqttConnectMessage
{
    bool        clean_session = true;
    uint16_t    keep_alive    = 0;
    std::string client_id;

    std::optional<MqttWillMessage> will;
    std::optional<std::string>     username;
    std::optional<std::string>     password;
};

/**********************
 *****Mqtt Connack*****
 **********************/

enum class MqttConnackReturnCode : uint8_t
{
    ACCEPTED                      = 0,
    UNACCEPTABLE_PROTOCOL_VERSION = 1,
    IDENTIFIER_REJECTED           = 2,
    SERVER_UNAVAILABLE            = 3,
    BAD_USERNAME_OR_PASSWORD      = 4,
    NOT_AUTHORIZED                = 5
};

struct MqttConnackMessage
{
    bool                  session_present = false;
    MqttConnackReturnCode return_code     = MqttConnackReturnCode::ACCEPTED;
};

/**********************
 *****Mqtt Publish*****
 **********************/

struct MqttPublishMessage
{
    bool dup = false;
    uint8_t qos = 0;
    bool retain = false;

    std::string topic_name;
    std::optional<uint16_t> packet_identifier; // Only present if QoS > 0
    std::span<const std::byte> payload;
};

/**********************
 ******Mqtt Puback*****
 **********************/

struct MqttPubackMessage
{
    uint16_t packet_identifier;
};

/**********************
 ******Mqtt Pubrec*****
 **********************/

struct MqttPubrecMessage
{
    uint16_t packet_identifier;
};

/**********************
 ******Mqtt Pubrel*****
 **********************/

struct MqttPubrelMessage
{
    uint16_t packet_identifier;
};

/**********************
 ******Mqtt Pubcomp****
 **********************/

struct MqttPubcompMessage
{
    uint16_t packet_identifier;
};

/**********************
 *****Mqtt Subscribe***
 **********************/

struct MqttSubscribeTopic
{
    std::string topic_filter;
    uint8_t     qos = 0;
};

struct MqttSubscribeMessage
{
    uint16_t packet_identifier;
    std::vector<MqttSubscribeTopic> topics;
};

/**********************
 ******Mqtt Suback*****
 **********************/

enum class MqttSubackReturnCode : uint8_t
{
    SUCCESS_QOS_0 = 0x00,
    SUCCESS_QOS_1 = 0x01,
    SUCCESS_QOS_2 = 0x02,
    FAILURE       = 0x80
};

struct MqttSubackMessage
{
    uint16_t packet_identifier;
    std::vector<MqttSubackReturnCode> return_codes;
};

/**********************
 ***Mqtt Unsubscribe***
 **********************/

struct MqttUnsubscribeMessage
{
    uint16_t packet_identifier;
    std::vector<std::string> topic_filters;
};

/**********************
 ***Mqtt Unsuback******
 **********************/

struct MqttUnsubackMessage
{
    uint16_t packet_identifier;
};

/**********************
 ******Mqtt Pingreq****
 **********************/

struct MqttPingreqMessage
{
};

/**********************
 ******Mqtt Pingresp***
 **********************/

struct MqttPingrespMessage
{
};

/**********************
 *****Mqtt Disconnect**
 **********************/

struct MqttDisconnectMessage
{
};