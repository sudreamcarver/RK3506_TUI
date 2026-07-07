#ifndef RK3506_TCP_PROTOCOL_HPP
#define RK3506_TCP_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rk3506_tcp
{

constexpr std::uint32_t kMagic = 0x524B3335U;
constexpr std::uint16_t kVersion = 1U;
constexpr std::uint16_t kHeaderSize = 36U;
constexpr std::uint32_t kMaxDatagramLength = 1200U;
constexpr std::uint32_t kMaxPayloadLength = kMaxDatagramLength - kHeaderSize;

enum class MessageType : std::uint16_t
{
    L2Hello = 0x0001,
    L2Heartbeat = 0x0002,
    L2Status = 0x0003,
    L3Join = 0x0101,
    L3Leave = 0x0102,
    L3Heartbeat = 0x0103,
    L3Status = 0x0104,
    NodeInfo = 0x0200,
    SensorData = 0x0201,
    SensorSummary = 0x0202,
    SensorConfig = 0x0203,
    Log = 0x0301,
    Alarm = 0x0302,
    Event = 0x0303,
    Command = 0x0401,
    CommandAck = 0x0402,
    Error = 0x04FF,
};

struct NodeInfoSummary
{
    std::uint32_t l2_id = 0U;
    std::uint32_t l3_id = 0U;
    std::uint32_t sensor_id = 0U;
    std::string name;
    std::string data_type;
    std::uint16_t data_type_id = 0U;
};

struct FrameHeader
{
    std::uint32_t magic = kMagic;
    std::uint16_t version = kVersion;
    std::uint16_t msg_type = 0U;
    std::uint16_t header_size = kHeaderSize;
    std::uint16_t flags = 0U;
    std::uint32_t l2_id = 0U;
    std::uint32_t seq = 0U;
    std::uint64_t timestamp_us = 0U;
    std::uint32_t payload_len = 0U;
    std::uint32_t payload_crc32 = 0U;
};

struct DataArraySummary
{
    std::uint16_t array_id = 0U;
    std::string name;
    std::string unit;
    std::uint16_t unit_id = 0U;
    std::vector<double> values;
};

struct SensorDataSummary
{
    std::uint32_t l2_id = 0U;
    std::uint32_t l3_id = 0U;
    std::uint32_t sensor_id = 0U;
    std::string data_type;
    std::uint16_t data_type_id = 0U;
    std::string data_format;
    std::uint16_t data_format_id = 0U;
    std::uint32_t sample_count = 0U;
    std::uint32_t array_count = 0U;
    std::vector<DataArraySummary> arrays;
};

std::uint64_t now_us();
std::string message_type_name(std::uint16_t msg_type);
std::string data_type_name(std::uint16_t data_type);
std::string data_format_name(std::uint16_t data_format);
std::string unit_name(std::uint16_t unit);
std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t length);
std::vector<std::uint8_t> encode_frame(
    const FrameHeader &header,
    const std::vector<std::uint8_t> &payload);
bool decode_header(const std::uint8_t *buffer,
                   std::size_t length,
                   FrameHeader *header,
                   std::string *error);
bool validate_header(const FrameHeader &header, std::string *error);
bool validate_frame_payload(const FrameHeader &header,
                            const std::uint8_t *payload,
                            std::size_t payload_len,
                            std::string *error);
bool parse_sensor_data_binary(const FrameHeader &header,
                              const std::uint8_t *payload,
                              std::size_t payload_len,
                              SensorDataSummary *summary,
                              std::string *error);
bool parse_node_info_binary(const FrameHeader &header,
                            const std::uint8_t *payload,
                            std::size_t payload_len,
                            NodeInfoSummary *summary,
                            std::string *error);
std::vector<std::uint8_t> make_node_info_payload_binary(
    std::uint32_t l3_id,
    std::uint32_t sensor_id,
    std::uint16_t data_type,
    float sample_rate_hz,
    const std::string &name,
    const std::vector<DataArraySummary> &arrays);
std::vector<std::uint8_t> make_sensor_payload_binary(
    std::uint32_t l3_id,
    std::uint32_t sensor_id,
    std::uint16_t data_type,
    std::uint16_t data_format,
    std::uint16_t sample_count,
    std::uint64_t sample_timestamp_us,
    std::uint64_t forward_timestamp_us,
    std::uint16_t quality,
    const std::vector<DataArraySummary> &arrays);

} // namespace rk3506_tcp

#endif
