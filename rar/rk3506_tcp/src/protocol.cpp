#include "rk3506_tcp_protocol.hpp"

#include <chrono>
#include <cstring>
#include <sstream>

namespace rk3506_tcp
{
namespace
{

constexpr std::uint16_t kFormatFloat32 = 0x0005U;

void append_u16(std::vector<std::uint8_t> *buffer, std::uint16_t value)
{
    buffer->push_back(static_cast<std::uint8_t>(value & 0xFFU));
    buffer->push_back(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
}

void append_u32(std::vector<std::uint8_t> *buffer, std::uint32_t value)
{
    append_u16(buffer, static_cast<std::uint16_t>(value & 0xFFFFU));
    append_u16(buffer, static_cast<std::uint16_t>((value >> 16U) & 0xFFFFU));
}

void append_u64(std::vector<std::uint8_t> *buffer, std::uint64_t value)
{
    append_u32(buffer, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
    append_u32(buffer, static_cast<std::uint32_t>((value >> 32U) & 0xFFFFFFFFULL));
}

void append_float32(std::vector<std::uint8_t> *buffer, float value)
{
    std::uint32_t raw = 0U;
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(buffer, raw);
}

std::uint16_t read_u16(const std::uint8_t *buffer)
{
    return static_cast<std::uint16_t>(buffer[0])
           | static_cast<std::uint16_t>(buffer[1] << 8U);
}

std::uint32_t read_u32(const std::uint8_t *buffer)
{
    return static_cast<std::uint32_t>(read_u16(buffer))
           | (static_cast<std::uint32_t>(read_u16(buffer + 2)) << 16U);
}

std::uint64_t read_u64(const std::uint8_t *buffer)
{
    return static_cast<std::uint64_t>(read_u32(buffer))
           | (static_cast<std::uint64_t>(read_u32(buffer + 4)) << 32U);
}

float read_float32(const std::uint8_t *buffer)
{
    const auto raw = read_u32(buffer);
    float value = 0.0F;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

bool require_remaining(std::size_t offset,
                       std::size_t required,
                       std::size_t length,
                       std::string *error)
{
    if (offset + required <= length)
        {
            return true;
        }
    if (error != nullptr)
        {
            *error = "payload too short";
        }
    return false;
}

std::string default_array_name(std::uint16_t data_type, std::uint16_t array_id)
{
    if (data_type == 0x0001U && array_id == 0U)
        {
            return "voltage_v";
        }
    if (data_type == 0x0002U && array_id == 0U)
        {
            return "current_a";
        }
    if (data_type == 0x0005U && array_id == 0U)
        {
            return "temperature_c";
        }
    if (data_type == 0x0104U)
        {
            static const char *names[] = {
                "voltage_v", "current_a", "soc_percent", "temperature_c",
                "cycle_count",
            };
            if (array_id < 5U)
                {
                    return names[array_id];
                }
        }
    if (data_type == 0x0101U)
        {
            static const char *names[] = {
                "rpm", "current_a", "voltage_v", "temperature_c",
                "load_percent",
            };
            if (array_id < 5U)
                {
                    return names[array_id];
                }
        }
    if (data_type == 0x0201U)
        {
            static const char *names[] = {
                "accel_x", "accel_y", "accel_z", "gyro_x", "gyro_y",
                "gyro_z", "mag_x", "mag_y", "mag_z", "temperature_c",
            };
            if (array_id < 10U)
                {
                    return names[array_id];
                }
        }

    return "array_" + std::to_string(array_id);
}

} // namespace

std::uint64_t now_us()
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::string message_type_name(std::uint16_t msg_type)
{
    switch (static_cast<MessageType>(msg_type))
        {
        case MessageType::L2Hello:
            return "L2_HELLO";
        case MessageType::L2Heartbeat:
            return "L2_HEARTBEAT";
        case MessageType::L2Status:
            return "L2_STATUS";
        case MessageType::L3Join:
            return "L3_JOIN";
        case MessageType::L3Leave:
            return "L3_LEAVE";
        case MessageType::L3Heartbeat:
            return "L3_HEARTBEAT";
        case MessageType::L3Status:
            return "L3_STATUS";
        case MessageType::NodeInfo:
            return "NODE_INFO";
        case MessageType::SensorData:
            return "SENSOR_DATA";
        case MessageType::SensorSummary:
            return "SENSOR_SUMMARY";
        case MessageType::SensorConfig:
            return "SENSOR_CONFIG";
        case MessageType::Log:
            return "LOG";
        case MessageType::Alarm:
            return "ALARM";
        case MessageType::Event:
            return "EVENT";
        case MessageType::Command:
            return "COMMAND";
        case MessageType::CommandAck:
            return "COMMAND_ACK";
        case MessageType::Error:
            return "ERROR";
        }

    std::ostringstream stream;
    stream << "UNKNOWN(0x" << std::hex << std::uppercase << msg_type << ")";
    return stream.str();
}

std::string data_type_name(std::uint16_t data_type)
{
    switch (data_type)
        {
        case 0x0001U:
            return "VOLTAGE";
        case 0x0002U:
            return "CURRENT";
        case 0x0003U:
            return "POWER";
        case 0x0004U:
            return "ENERGY";
        case 0x0005U:
            return "TEMPERATURE";
        case 0x0009U:
            return "DISTANCE";
        case 0x000AU:
            return "SWITCH";
        case 0x0101U:
            return "MOTOR";
        case 0x0104U:
            return "BATTERY";
        case 0x0201U:
            return "IMU";
        case 0x0301U:
            return "GNSS";
        case 0x0401U:
            return "RADAR_SUMMARY";
        default:
            return "UNKNOWN";
        }
}

std::string data_format_name(std::uint16_t data_format)
{
    switch (data_format)
        {
        case 0x0001U:
            return "INT16";
        case 0x0002U:
            return "UINT16";
        case 0x0003U:
            return "INT32";
        case 0x0004U:
            return "UINT32";
        case 0x0005U:
            return "FLOAT32";
        case 0x0006U:
            return "FLOAT64";
        default:
            return "UNKNOWN";
        }
}

std::string unit_name(std::uint16_t unit)
{
    switch (unit)
        {
        case 0x0000U:
            return "NONE";
        case 0x0001U:
            return "VOLT";
        case 0x0002U:
            return "AMPERE";
        case 0x0003U:
            return "CELSIUS";
        case 0x0004U:
            return "PERCENT";
        case 0x0005U:
            return "RPM";
        case 0x0006U:
            return "METER";
        case 0x0008U:
            return "METER_PER_SEC";
        case 0x0009U:
            return "METER_PER_SEC2";
        case 0x000AU:
            return "DEGREE_PER_SEC";
        case 0x000BU:
            return "DEGREE";
        case 0x0011U:
            return "MICRO_TESLA";
        case 0x0013U:
            return "BOOLEAN";
        case 0x0014U:
            return "COUNT";
        case 0x0016U:
            return "HERTZ";
        case 0x0017U:
            return "WATT";
        case 0x0018U:
            return "WATT_HOUR";
        case 0x0019U:
            return "NEWTON";
        default:
            return "UNKNOWN";
        }
}

std::uint32_t crc32_ieee(const std::uint8_t *data, std::size_t length)
{
    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t i = 0U; i < length; ++i)
        {
            crc ^= data[i];
            for (int bit = 0; bit < 8; ++bit)
                {
                    if ((crc & 1U) != 0U)
                        {
                            crc = (crc >> 1U) ^ 0xEDB88320U;
                        }
                    else
                        {
                            crc >>= 1U;
                        }
                }
        }
    return crc ^ 0xFFFFFFFFU;
}

std::vector<std::uint8_t> encode_frame(
    const FrameHeader &header,
    const std::vector<std::uint8_t> &payload)
{
    FrameHeader encoded_header = header;
    encoded_header.magic = kMagic;
    encoded_header.version = kVersion;
    encoded_header.header_size = kHeaderSize;
    encoded_header.payload_len = static_cast<std::uint32_t>(payload.size());
    encoded_header.payload_crc32 = crc32_ieee(payload.data(), payload.size());

    std::vector<std::uint8_t> frame;
    frame.reserve(kHeaderSize + payload.size());
    append_u32(&frame, encoded_header.magic);
    append_u16(&frame, encoded_header.version);
    append_u16(&frame, encoded_header.msg_type);
    append_u16(&frame, encoded_header.header_size);
    append_u16(&frame, encoded_header.flags);
    append_u32(&frame, encoded_header.l2_id);
    append_u32(&frame, encoded_header.seq);
    append_u64(&frame, encoded_header.timestamp_us);
    append_u32(&frame, encoded_header.payload_len);
    append_u32(&frame, encoded_header.payload_crc32);
    frame.insert(frame.end(), payload.begin(), payload.end());
    return frame;
}

bool decode_header(const std::uint8_t *buffer,
                   std::size_t length,
                   FrameHeader *header,
                   std::string *error)
{
    if (length < kHeaderSize)
        {
            if (error != nullptr)
                {
                    *error = "not enough bytes for FrameHeader";
                }
            return false;
        }

    header->magic = read_u32(buffer);
    header->version = read_u16(buffer + 4);
    header->msg_type = read_u16(buffer + 6);
    header->header_size = read_u16(buffer + 8);
    header->flags = read_u16(buffer + 10);
    header->l2_id = read_u32(buffer + 12);
    header->seq = read_u32(buffer + 16);
    header->timestamp_us = read_u64(buffer + 20);
    header->payload_len = read_u32(buffer + 28);
    header->payload_crc32 = read_u32(buffer + 32);
    return validate_header(*header, error);
}

bool validate_header(const FrameHeader &header, std::string *error)
{
    if (header.magic != kMagic)
        {
            if (error != nullptr)
                {
                    *error = "invalid magic";
                }
            return false;
        }
    if (header.version != kVersion)
        {
            if (error != nullptr)
                {
                    *error = "unsupported protocol version";
                }
            return false;
        }
    if (header.header_size != kHeaderSize)
        {
            if (error != nullptr)
                {
                    *error = "invalid header size";
                }
            return false;
        }
    if (header.payload_len > kMaxPayloadLength)
        {
            if (error != nullptr)
                {
                    *error = "payload too large";
                }
            return false;
        }
    return true;
}

bool validate_frame_payload(const FrameHeader &header,
                            const std::uint8_t *payload,
                            std::size_t payload_len,
                            std::string *error)
{
    if (payload_len != header.payload_len)
        {
            if (error != nullptr)
                {
                    *error = "payload length mismatch";
                }
            return false;
        }

    const auto crc = crc32_ieee(payload, payload_len);
    if (crc != header.payload_crc32)
        {
            if (error != nullptr)
                {
                    *error = "payload crc32 mismatch";
                }
            return false;
        }

    return true;
}

bool parse_sensor_data_binary(const FrameHeader &header,
                              const std::uint8_t *payload,
                              std::size_t payload_len,
                              SensorDataSummary *summary,
                              std::string *error)
{
    if (summary == nullptr)
        {
            return false;
        }
    if (!require_remaining(0U, 36U, payload_len, error))
        {
            return false;
        }

    SensorDataSummary parsed;
    parsed.l2_id = header.l2_id;
    parsed.l3_id = read_u32(payload);
    parsed.sensor_id = read_u32(payload + 4);
    parsed.data_type_id = read_u16(payload + 8);
    parsed.data_type = data_type_name(parsed.data_type_id);
    parsed.data_format_id = read_u16(payload + 10);
    parsed.data_format = data_format_name(parsed.data_format_id);
    parsed.sample_count = read_u16(payload + 12);
    parsed.array_count = read_u16(payload + 14);

    std::size_t offset = 36U;
    for (std::uint32_t index = 0U; index < parsed.array_count; ++index)
        {
            if (!require_remaining(offset, 8U, payload_len, error))
                {
                    return false;
                }

            DataArraySummary array;
            array.array_id = read_u16(payload + offset);
            array.unit_id = read_u16(payload + offset + 2);
            const auto value_count = read_u16(payload + offset + 4);
            array.unit = unit_name(array.unit_id);
            array.name = default_array_name(parsed.data_type_id, array.array_id);
            offset += 8U;

            if (parsed.data_format_id != kFormatFloat32)
                {
                    if (error != nullptr)
                        {
                            *error = "unsupported data format";
                        }
                    return false;
                }

            const auto values_len = static_cast<std::size_t>(value_count) * 4U;
            if (!require_remaining(offset, values_len, payload_len, error))
                {
                    return false;
                }
            for (std::uint16_t value_index = 0U; value_index < value_count;
                 ++value_index)
                {
                    array.values.push_back(
                        static_cast<double>(read_float32(payload + offset)));
                    offset += 4U;
                }

            parsed.arrays.push_back(array);
        }

    if (offset != payload_len)
        {
            if (error != nullptr)
                {
                    *error = "trailing bytes in SENSOR_DATA payload";
                }
            return false;
        }

    *summary = parsed;
    return true;
}

bool parse_node_info_binary(const FrameHeader &header,
                            const std::uint8_t *payload,
                            std::size_t payload_len,
                            NodeInfoSummary *summary,
                            std::string *error)
{
    if (summary == nullptr)
        {
            return false;
        }
    if (!require_remaining(0U, 18U, payload_len, error))
        {
            return false;
        }

    NodeInfoSummary parsed;
    parsed.l2_id = header.l2_id;
    parsed.l3_id = read_u32(payload);
    parsed.sensor_id = read_u32(payload + 4);
    parsed.data_type_id = read_u16(payload + 8);
    parsed.data_type = data_type_name(parsed.data_type_id);
    const auto array_count = read_u16(payload + 10);
    const auto name_len = read_u16(payload + 16);

    std::size_t offset = 18U;
    if (!require_remaining(offset, name_len, payload_len, error))
        {
            return false;
        }
    parsed.name = std::string(
        reinterpret_cast<const char *>(payload + offset), name_len);
    offset += name_len;

    for (std::uint16_t index = 0U; index < array_count; ++index)
        {
            if (!require_remaining(offset, 6U, payload_len, error))
                {
                    return false;
                }
            const auto descriptor_name_len = read_u16(payload + offset + 4);
            offset += 6U;
            if (!require_remaining(offset, descriptor_name_len, payload_len, error))
                {
                    return false;
                }
            offset += descriptor_name_len;
        }

    if (offset != payload_len)
        {
            if (error != nullptr)
                {
                    *error = "trailing bytes in NODE_INFO payload";
                }
            return false;
        }

    if (parsed.name.empty())
        {
            parsed.name = "S" + std::to_string(parsed.sensor_id) + " "
                          + parsed.data_type;
        }

    *summary = parsed;
    return true;
}

std::vector<std::uint8_t> make_node_info_payload_binary(
    std::uint32_t l3_id,
    std::uint32_t sensor_id,
    std::uint16_t data_type,
    float sample_rate_hz,
    const std::string &name,
    const std::vector<DataArraySummary> &arrays)
{
    std::vector<std::uint8_t> payload;
    append_u32(&payload, l3_id);
    append_u32(&payload, sensor_id);
    append_u16(&payload, data_type);
    append_u16(&payload, static_cast<std::uint16_t>(arrays.size()));
    append_float32(&payload, sample_rate_hz);
    append_u16(&payload, static_cast<std::uint16_t>(name.size()));
    payload.insert(payload.end(), name.begin(), name.end());

    for (const auto &array : arrays)
        {
            append_u16(&payload, array.array_id);
            append_u16(&payload, array.unit_id);
            append_u16(&payload, static_cast<std::uint16_t>(array.name.size()));
            payload.insert(payload.end(), array.name.begin(), array.name.end());
        }

    return payload;
}

std::vector<std::uint8_t> make_sensor_payload_binary(
    std::uint32_t l3_id,
    std::uint32_t sensor_id,
    std::uint16_t data_type,
    std::uint16_t data_format,
    std::uint16_t sample_count,
    std::uint64_t sample_timestamp_us,
    std::uint64_t forward_timestamp_us,
    std::uint16_t quality,
    const std::vector<DataArraySummary> &arrays)
{
    std::vector<std::uint8_t> payload;
    append_u32(&payload, l3_id);
    append_u32(&payload, sensor_id);
    append_u16(&payload, data_type);
    append_u16(&payload, data_format);
    append_u16(&payload, sample_count);
    append_u16(&payload, static_cast<std::uint16_t>(arrays.size()));
    append_u64(&payload, sample_timestamp_us);
    append_u64(&payload, forward_timestamp_us);
    append_u16(&payload, quality);
    append_u16(&payload, 0U);

    for (const auto &array : arrays)
        {
            append_u16(&payload, array.array_id);
            append_u16(&payload, array.unit_id);
            append_u16(&payload, static_cast<std::uint16_t>(array.values.size()));
            append_u16(&payload, 0U);
            for (const auto value : array.values)
                {
                    append_float32(&payload, static_cast<float>(value));
                }
        }

    return payload;
}

} // namespace rk3506_tcp
