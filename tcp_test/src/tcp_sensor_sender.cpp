#include "rk3506_tcp_protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace
{

struct Config
{
    std::string host = "127.0.0.1";
    std::uint16_t port = 35060U;
    std::string message = "sensor-data";
    std::string type = "imu";
    std::uint32_t l2_id = 1U;
    std::uint32_t l3_id = 11U;
    std::uint32_t sensor_id = 0U;
    std::uint32_t sensor_id_step = 0U;
    int count = 1;
    int interval_ms = 1000;
};

void print_usage(const char *program)
{
    std::cout << "Usage: " << program << " [options]\n"
              << "\n"
              << "Options:\n"
              << "  --host HOST           Server host. Default: 127.0.0.1\n"
              << "  --port PORT           Server port. Default: 35060\n"
              << "  --message MSG         sensor-data/node-info/l2-hello/l2-heartbeat. Default: sensor-data\n"
              << "  --type TYPE           voltage/current/temperature/imu/motor/battery\n"
              << "  --l2-id ID            L2 controller ID. Default: 1\n"
              << "  --l3-id ID            L3 node ID. Default: 11\n"
              << "  --sensor-id ID        Sensor ID. Default depends on --type\n"
              << "  --sensor-id-step N    Add N to sensor ID for each frame. Default: 0\n"
              << "  --count N             Number of frames. Default: 1\n"
              << "  --interval-ms MS      Delay between frames. Default: 1000\n"
              << "  -h, --help            Show this help\n";
}

bool parse_u16(const char *value, std::uint16_t *out)
{
    try
        {
            const auto parsed = std::stoul(value);
            if (parsed == 0U || parsed > 65535U)
                {
                    return false;
                }
            *out = static_cast<std::uint16_t>(parsed);
            return true;
        }
    catch (...)
        {
            return false;
        }
}

bool parse_int(const char *value, int *out)
{
    try
        {
            const auto parsed = std::stoi(value);
            if (parsed < 0)
                {
                    return false;
                }
            *out = parsed;
            return true;
        }
    catch (...)
        {
            return false;
    }
}

bool parse_u32(const char *value, std::uint32_t *out)
{
    try
        {
            const auto parsed = std::stoul(value);
            if (parsed > 0xFFFFFFFFUL)
                {
                    return false;
                }
            *out = static_cast<std::uint32_t>(parsed);
            return true;
        }
    catch (...)
        {
            return false;
        }
}

bool parse_args(int argc, char **argv, Config *config)
{
    for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "-h" || arg == "--help")
                {
                    print_usage(argv[0]);
                    return false;
                }
            if (arg == "--host")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --host value\n";
                            return false;
                        }
                    config->host = argv[i];
                }
            else if (arg == "--port")
                {
                    if (++i >= argc || !parse_u16(argv[i], &config->port))
                        {
                            std::cerr << "invalid --port value\n";
                            return false;
                        }
                }
            else if (arg == "--type")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --type value\n";
                            return false;
                        }
                    config->type = argv[i];
                }
            else if (arg == "--message")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --message value\n";
                            return false;
                        }
                    config->message = argv[i];
                }
            else if (arg == "--l2-id")
                {
                    if (++i >= argc || !parse_u32(argv[i], &config->l2_id))
                        {
                            std::cerr << "invalid --l2-id value\n";
                            return false;
                        }
                }
            else if (arg == "--l3-id")
                {
                    if (++i >= argc || !parse_u32(argv[i], &config->l3_id))
                        {
                            std::cerr << "invalid --l3-id value\n";
                            return false;
                        }
                }
            else if (arg == "--sensor-id")
                {
                    if (++i >= argc || !parse_u32(argv[i], &config->sensor_id)
                        || config->sensor_id == 0U)
                        {
                            std::cerr << "invalid --sensor-id value\n";
                            return false;
                        }
                }
            else if (arg == "--sensor-id-step")
                {
                    if (++i >= argc
                        || !parse_u32(argv[i], &config->sensor_id_step))
                        {
                            std::cerr << "invalid --sensor-id-step value\n";
                            return false;
                        }
                }
            else if (arg == "--count")
                {
                    if (++i >= argc || !parse_int(argv[i], &config->count))
                        {
                            std::cerr << "invalid --count value\n";
                            return false;
                        }
                }
            else if (arg == "--interval-ms")
                {
                    if (++i >= argc
                        || !parse_int(argv[i], &config->interval_ms))
                        {
                            std::cerr << "invalid --interval-ms value\n";
                            return false;
                        }
                }
            else
                {
                    std::cerr << "unknown option: " << arg << '\n';
                    return false;
                }
        }

    return true;
}

rk3506_tcp::DataArraySummary array(std::uint16_t id,
                                   std::string name,
                                   std::uint16_t unit_id,
                                   double value)
{
    rk3506_tcp::DataArraySummary result;
    result.array_id = id;
    result.name = std::move(name);
    result.unit_id = unit_id;
    result.unit = rk3506_tcp::unit_name(unit_id);
    result.values.push_back(value);
    return result;
}

std::uint16_t data_type_id_for_type(const std::string &type)
{
    if (type == "voltage")
        {
            return 0x0001U;
        }
    if (type == "current")
        {
            return 0x0002U;
        }
    if (type == "temperature")
        {
            return 0x0005U;
        }
    if (type == "motor")
        {
            return 0x0101U;
        }
    if (type == "battery")
        {
            return 0x0104U;
        }
    return 0x0201U;
}

std::uint32_t default_sensor_id_for_type(const std::string &type)
{
    if (type == "voltage")
        {
            return 3U;
        }
    if (type == "current")
        {
            return 4U;
        }
    if (type == "temperature")
        {
            return 5U;
        }
    if (type == "motor")
        {
            return 6U;
        }
    if (type == "battery")
        {
            return 7U;
        }
    return 1U;
}

std::vector<std::uint8_t> make_payload(const Config &config, int frame_index)
{
    using rk3506_tcp::DataArraySummary;

    const auto &type = config.type;
    const auto offset = static_cast<double>(frame_index);
    std::vector<DataArraySummary> arrays;
    std::string data_type;
    std::uint32_t sensor_id = default_sensor_id_for_type(type);

    if (type == "voltage")
        {
            data_type = "VOLTAGE";
            arrays.push_back(array(0U, "voltage_v", 0x0001U, 24.0 + offset));
        }
    else if (type == "current")
        {
            data_type = "CURRENT";
            arrays.push_back(array(0U, "current_a", 0x0002U, 1.2 + offset));
        }
    else if (type == "temperature")
        {
            data_type = "TEMPERATURE";
            arrays.push_back(
                array(0U, "temperature_c", 0x0003U, 42.0 + offset));
        }
    else if (type == "motor")
        {
            data_type = "MOTOR";
            arrays.push_back(array(0U, "rpm", 0x0005U, 1200.0 + offset));
            arrays.push_back(array(1U, "current_a", 0x0002U, 1.25));
            arrays.push_back(array(2U, "voltage_v", 0x0001U, 24.1));
            arrays.push_back(array(3U, "temperature_c", 0x0003U, 42.0));
            arrays.push_back(array(4U, "load_percent", 0x0004U, 38.0));
        }
    else if (type == "battery")
        {
            data_type = "BATTERY";
            arrays.push_back(array(0U, "voltage_v", 0x0001U, 24.1));
            arrays.push_back(array(1U, "current_a", 0x0002U, 1.25));
            arrays.push_back(array(2U, "soc_percent", 0x0004U, 86.0));
            arrays.push_back(array(3U, "temperature_c", 0x0003U, 36.5));
            arrays.push_back(array(4U, "cycle_count", 0x0014U, 25.0));
        }
    else
        {
            data_type = "IMU";
            arrays.push_back(array(0U, "accel_x", 0x0009U, 0.01));
            arrays.push_back(array(1U, "accel_y", 0x0009U, 0.02));
            arrays.push_back(array(2U, "accel_z", 0x0009U, 9.80));
            arrays.push_back(array(3U, "gyro_x", 0x000AU, 0.00));
            arrays.push_back(array(4U, "gyro_y", 0x000AU, 0.01));
            arrays.push_back(array(5U, "gyro_z", 0x000AU, 0.00));
            arrays.push_back(array(6U, "mag_x", 0x0011U, 12.3));
            arrays.push_back(array(7U, "mag_y", 0x0011U, -4.2));
            arrays.push_back(array(8U, "mag_z", 0x0011U, 36.8));
            arrays.push_back(array(9U, "temperature_c", 0x0003U, 38.5));
        }

    if (config.sensor_id != 0U)
        {
            sensor_id = config.sensor_id;
        }
    sensor_id += config.sensor_id_step
                 * static_cast<std::uint32_t>(frame_index);

    return rk3506_tcp::make_sensor_payload_binary(
        config.l3_id,
        sensor_id,
        data_type_id_for_type(type),
        0x0005U,
        1U,
        rk3506_tcp::now_us(),
        rk3506_tcp::now_us(),
        1000U,
        arrays);
}

std::vector<std::uint8_t> make_node_info_payload(const Config &config,
                                                 int frame_index)
{
    const auto sensor_id
        = (config.sensor_id == 0U ? default_sensor_id_for_type(config.type)
                                  : config.sensor_id)
          + config.sensor_id_step * static_cast<std::uint32_t>(frame_index);
    std::string data_type = "IMU";
    if (config.type == "voltage")
        {
            data_type = "VOLTAGE";
        }
    else if (config.type == "current")
        {
            data_type = "CURRENT";
        }
    else if (config.type == "temperature")
        {
            data_type = "TEMPERATURE";
        }
    else if (config.type == "motor")
        {
            data_type = "MOTOR";
        }
    else if (config.type == "battery")
        {
            data_type = "BATTERY";
        }

    std::vector<rk3506_tcp::DataArraySummary> arrays;
    if (config.type == "voltage")
        {
            arrays.push_back(array(0U, "voltage_v", 0x0001U, 0.0));
        }
    else if (config.type == "current")
        {
            arrays.push_back(array(0U, "current_a", 0x0002U, 0.0));
        }
    else if (config.type == "temperature")
        {
            arrays.push_back(array(0U, "temperature_c", 0x0003U, 0.0));
        }
    else if (config.type == "motor")
        {
            arrays.push_back(array(0U, "rpm", 0x0005U, 0.0));
            arrays.push_back(array(1U, "current_a", 0x0002U, 0.0));
            arrays.push_back(array(2U, "voltage_v", 0x0001U, 0.0));
            arrays.push_back(array(3U, "temperature_c", 0x0003U, 0.0));
            arrays.push_back(array(4U, "load_percent", 0x0004U, 0.0));
        }
    else if (config.type == "battery")
        {
            arrays.push_back(array(0U, "voltage_v", 0x0001U, 0.0));
            arrays.push_back(array(1U, "current_a", 0x0002U, 0.0));
            arrays.push_back(array(2U, "soc_percent", 0x0004U, 0.0));
            arrays.push_back(array(3U, "temperature_c", 0x0003U, 0.0));
            arrays.push_back(array(4U, "cycle_count", 0x0014U, 0.0));
        }
    else
        {
            arrays.push_back(array(0U, "accel_x", 0x0009U, 0.0));
            arrays.push_back(array(1U, "accel_y", 0x0009U, 0.0));
            arrays.push_back(array(2U, "accel_z", 0x0009U, 0.0));
            arrays.push_back(array(3U, "gyro_x", 0x000AU, 0.0));
            arrays.push_back(array(4U, "gyro_y", 0x000AU, 0.0));
            arrays.push_back(array(5U, "gyro_z", 0x000AU, 0.0));
            arrays.push_back(array(6U, "mag_x", 0x0011U, 0.0));
            arrays.push_back(array(7U, "mag_y", 0x0011U, 0.0));
            arrays.push_back(array(8U, "mag_z", 0x0011U, 0.0));
            arrays.push_back(array(9U, "temperature_c", 0x0003U, 0.0));
        }

    return rk3506_tcp::make_node_info_payload_binary(
        config.l3_id,
        sensor_id,
        data_type_id_for_type(config.type),
        100.0F,
        "S" + std::to_string(sensor_id) + " " + data_type,
        arrays);
}

std::vector<std::uint8_t> make_l2_payload(const Config &)
{
    return {};
}

std::uint16_t message_type(const Config &config)
{
    if (config.message == "node-info")
        {
            return static_cast<std::uint16_t>(
                rk3506_tcp::MessageType::NodeInfo);
        }
    if (config.message == "l2-heartbeat")
        {
            return static_cast<std::uint16_t>(
                rk3506_tcp::MessageType::L2Heartbeat);
        }
    if (config.message == "l2-hello")
        {
            return static_cast<std::uint16_t>(
                rk3506_tcp::MessageType::L2Hello);
        }
    return static_cast<std::uint16_t>(rk3506_tcp::MessageType::SensorData);
}

std::vector<std::uint8_t> make_frame_payload(const Config &config,
                                             int frame_index)
{
    if (config.message == "node-info")
        {
            return make_node_info_payload(config, frame_index);
        }
    if (config.message == "l2-heartbeat" || config.message == "l2-hello")
        {
            return make_l2_payload(config);
        }
    return make_payload(config, frame_index);
}

} // namespace

int main(int argc, char **argv)
{
    Config config;
    if (!parse_args(argc, argv, &config))
        {
            return 1;
        }

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        {
            std::cerr << "socket: " << std::strerror(errno) << '\n';
            return 1;
        }

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(config.port);
    if (inet_pton(AF_INET, config.host.c_str(), &address.sin_addr) != 1)
        {
            std::cerr << "invalid host address: " << config.host << '\n';
            close(fd);
            return 1;
        }

    for (int i = 0; i < config.count; ++i)
        {
            const auto payload = make_frame_payload(config, i);
            rk3506_tcp::FrameHeader header;
            header.msg_type = message_type(config);
            header.l2_id = config.l2_id;
            header.seq = static_cast<std::uint32_t>(i + 1);
            header.timestamp_us = rk3506_tcp::now_us();

            const auto frame = rk3506_tcp::encode_frame(header, payload);
            const auto sent = sendto(
                fd,
                frame.data(),
                frame.size(),
                0,
                reinterpret_cast<sockaddr *>(&address),
                sizeof(address));
            if (sent < 0
                || static_cast<std::size_t>(sent) != frame.size())
                {
                    std::cerr << "sendto: " << std::strerror(errno) << '\n';
                    close(fd);
                    return 1;
                }

            std::cout << "sent udp frame seq=" << (i + 1)
                      << " message=" << config.message
                      << " type=" << config.type
                      << " l2=" << config.l2_id
                      << " l3=" << config.l3_id
                      << " sensor="
                      << ((config.sensor_id == 0U
                               ? default_sensor_id_for_type(config.type)
                               : config.sensor_id)
                          + config.sensor_id_step
                                * static_cast<std::uint32_t>(i))
                      << " payload_len=" << payload.size() << '\n';

            if (i + 1 < config.count)
                {
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(config.interval_ms));
                }
        }

    close(fd);
    return 0;
}
