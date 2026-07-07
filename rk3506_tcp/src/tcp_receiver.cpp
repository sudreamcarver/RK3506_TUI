#include "rk3506_tcp_protocol.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <system_error>
#include <string>
#include <thread>
#include <vector>

namespace
{

volatile std::sig_atomic_t keep_running = 1;
std::mutex log_mutex;
std::mutex state_mutex;

struct L2Record
{
    std::uint32_t l2_id = 0U;
    std::uint64_t last_heartbeat_us = 0U;
};

struct SensorRecord
{
    std::uint32_t l2_id = 0U;
    std::uint32_t l3_id = 0U;
    std::uint32_t sensor_id = 0U;
    std::string name;
    std::string data_type;
    std::string latest_data;
    bool info_received = false;
    bool data_received = false;
};

std::map<std::uint32_t, L2Record> l2_records;
std::map<std::string, SensorRecord> sensor_records;

void handle_signal(int)
{
    keep_running = 0;
}

void print_usage(const char *program)
{
    std::cout << "Usage: " << program << " [--port PORT]\n"
              << "\n"
              << "Options:\n"
              << "  --port PORT     Listen port. Default: 35060\n"
              << "  --log-file PATH Append parsed receive log. Default: /tmp/rk3506_udp_received.log\n"
              << "  --state-file PATH Write latest node snapshot. Default: /tmp/rk3506_udp_nodes.tsv\n"
              << "  --delete-file PATH Read TUI delete requests. Default: /tmp/rk3506_udp_delete.tsv\n"
              << "  -h, --help      Show this help\n";
}

bool parse_port(const char *value, std::uint16_t *port)
{
    try
        {
            const auto parsed = std::stoul(value);
            if (parsed == 0U || parsed > 65535U)
                {
                    return false;
                }
            *port = static_cast<std::uint16_t>(parsed);
            return true;
        }
    catch (...)
        {
            return false;
        }
}

void append_log(const std::string &log_path, const std::string &message)
{
    std::lock_guard<std::mutex> lock(log_mutex);
    std::ofstream log(log_path, std::ios::app);
    if (log.is_open())
        {
            log << message;
            log.flush();
        }
}

std::uint64_t monotonic_us()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::string l2_key(std::uint32_t l2_id)
{
    return "L2-" + std::to_string(l2_id);
}

std::string sensor_key(std::uint32_t l2_id,
                       std::uint32_t l3_id,
                       std::uint32_t sensor_id)
{
    return l2_key(l2_id) + "/L3-" + std::to_string(l3_id)
           + "/S" + std::to_string(sensor_id);
}

void touch_l2(std::uint32_t l2_id)
{
    auto &record = l2_records[l2_id];
    record.l2_id = l2_id;
    record.last_heartbeat_us = monotonic_us();
}

bool l2_online(const L2Record &record)
{
    constexpr std::uint64_t timeout_us = 3ULL * 1000ULL * 1000ULL;
    return monotonic_us() - record.last_heartbeat_us <= timeout_us;
}

std::string escape_state_field(const std::string &value)
{
    std::string escaped;
    escaped.reserve(value.size());

    for (const auto ch : value)
        {
            if (ch == '\\')
                {
                    escaped += "\\\\";
                }
            else if (ch == '\n')
                {
                    escaped += "\\n";
                }
            else if (ch == '\t')
                {
                    escaped += "\\t";
                }
            else
                {
                    escaped.push_back(ch);
                }
        }

    return escaped;
}

void write_state_file_unlocked(const std::string &state_path)
{
    const auto temp_path = state_path + ".tmp";
    std::ofstream state(temp_path, std::ios::trunc);
    if (!state.is_open())
        {
            return;
        }

    for (const auto &entry : l2_records)
        {
            const auto online = l2_online(entry.second);
            const auto key = l2_key(entry.first);
            const auto display = std::string(online ? "[ON]  " : "[OFF] ")
                                 + key;
            const auto detail = std::string("状态: ")
                                + (online ? "Online" : "Offline") + "\nL2: "
                                + std::to_string(entry.first)
                                + "\nHeartbeat: "
                                + (online ? "active" : "timeout");

            state << escape_state_field(key) << '\t'
                  << escape_state_field(display) << '\t'
                  << escape_state_field(detail) << '\n';
        }

    for (const auto &entry : sensor_records)
        {
            const auto l2_it = l2_records.find(entry.second.l2_id);
            const auto online = l2_it != l2_records.end()
                                && l2_online(l2_it->second);
            const auto status = online ? "[ON]  " : "[OFF] ";
            const auto type = entry.second.data_type.empty()
                                  ? "UNKNOWN"
                                  : entry.second.data_type;
            const auto display = status + entry.first + " " + type;
            std::ostringstream detail;
            detail << "状态: " << (online ? "Online" : "Offline") << '\n';
            detail << "L2: " << entry.second.l2_id << '\n';
            detail << "L3: " << entry.second.l3_id << '\n';
            detail << "Sensor: " << entry.second.sensor_id << '\n';
            detail << "Type: " << type << '\n';
            detail << "Info: "
                   << (entry.second.info_received ? "received" : "missing")
                   << '\n';
            detail << "Data: "
                   << (entry.second.data_received ? "received" : "missing")
                   << '\n';
            if (!entry.second.latest_data.empty())
                {
                    detail << entry.second.latest_data;
                }

            state << escape_state_field(entry.first) << '\t'
                  << escape_state_field(display) << '\t'
                  << escape_state_field(detail.str()) << '\n';
        }

    state.close();
    std::rename(temp_path.c_str(), state_path.c_str());
}

void process_delete_file_unlocked(const std::string &delete_path,
                                  const std::string &log_path)
{
    std::ifstream deletes(delete_path);
    if (!deletes.is_open())
        {
            return;
        }

    std::string key;
    while (std::getline(deletes, key))
        {
            if (key.empty())
                {
                    continue;
                }

            append_log(log_path, "delete node request: " + key + "\n");

            if (key.rfind("L2-", 0) == 0 && key.find('/') == std::string::npos)
                {
                    std::uint32_t l2_id = 0U;
                    try
                        {
                            l2_id = static_cast<std::uint32_t>(
                                std::stoul(key.substr(3U)));
                        }
                    catch (...)
                        {
                            continue;
                        }
                    l2_records.erase(l2_id);

                    for (auto it = sensor_records.begin();
                         it != sensor_records.end();)
                        {
                            if (it->second.l2_id == l2_id)
                                {
                                    it = sensor_records.erase(it);
                                }
                            else
                                {
                                    ++it;
                                }
                        }
                }
            else
                {
                    sensor_records.erase(key);
                }
        }

    deletes.close();
    std::remove(delete_path.c_str());
}

std::string sensor_summary_text(const rk3506_tcp::SensorDataSummary &summary)
{
    std::ostringstream stream;
    stream << "  SENSOR_DATA"
           << " l2=" << summary.l2_id
           << " l3=" << summary.l3_id
           << " sensor=" << summary.sensor_id
           << " type=" << summary.data_type
           << " arrays=" << summary.array_count << '\n';

    for (const auto &array : summary.arrays)
        {
            stream << "    [" << array.array_id << "] " << array.name
                   << " (" << array.unit << "):";
            for (const auto value : array.values)
                {
                    stream << ' ' << value;
                }
            stream << '\n';
        }

    return stream.str();
}

std::string sensor_detail_text(const rk3506_tcp::SensorDataSummary &summary)
{
    std::ostringstream stream;
    stream << "Arrays: " << summary.array_count << '\n';

    for (const auto &array : summary.arrays)
        {
            stream << array.name << ":";
            for (const auto value : array.values)
                {
                    stream << ' ' << value;
                }
            stream << ' ' << array.unit << '\n';
        }

    return stream.str();
}

void update_node_snapshot(const std::string &state_path,
                          const rk3506_tcp::SensorDataSummary &summary)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    touch_l2(summary.l2_id);

    auto &record = sensor_records[sensor_key(
        summary.l2_id, summary.l3_id, summary.sensor_id)];
    record.l2_id = summary.l2_id;
    record.l3_id = summary.l3_id;
    record.sensor_id = summary.sensor_id;
    record.data_type = summary.data_type;
    if (record.name.empty())
        {
            record.name = "S" + std::to_string(summary.sensor_id);
        }
    record.latest_data = sensor_detail_text(summary);
    record.data_received = true;
    write_state_file_unlocked(state_path);
}

void update_node_info_snapshot(const std::string &state_path,
                               const rk3506_tcp::NodeInfoSummary &summary)
{
    std::lock_guard<std::mutex> lock(state_mutex);
    touch_l2(summary.l2_id);

    auto &record = sensor_records[sensor_key(
        summary.l2_id, summary.l3_id, summary.sensor_id)];
    record.l2_id = summary.l2_id;
    record.l3_id = summary.l3_id;
    record.sensor_id = summary.sensor_id;
    record.name = summary.name;
    record.data_type = summary.data_type;
    record.info_received = true;
    write_state_file_unlocked(state_path);
}

void handle_datagram(const std::uint8_t *datagram,
                     std::size_t datagram_len,
                     const sockaddr_in &peer,
                     const std::string &log_path,
                     const std::string &state_path)
{
    char address[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &peer.sin_addr, address, sizeof(address));

    rk3506_tcp::FrameHeader header;
    std::string error;
    if (!rk3506_tcp::decode_header(
            datagram, datagram_len, &header, &error))
        {
            std::cerr << "bad UDP frame from " << address << ':'
                      << ntohs(peer.sin_port) << ": " << error << '\n';
            return;
        }

    if (datagram_len > rk3506_tcp::kMaxDatagramLength)
        {
            std::cerr << "oversized UDP frame from " << address << ':'
                      << ntohs(peer.sin_port) << ": " << datagram_len
                      << " bytes\n";
            return;
        }

    const auto expected_len = static_cast<std::size_t>(header.header_size)
                              + static_cast<std::size_t>(header.payload_len);
    if (expected_len != datagram_len)
        {
            std::cerr << "bad UDP frame length from " << address << ':'
                      << ntohs(peer.sin_port) << ": expected "
                      << expected_len << " got " << datagram_len << '\n';
            return;
        }

    const auto *payload_begin = datagram + header.header_size;
    if (!rk3506_tcp::validate_frame_payload(
            header, payload_begin, header.payload_len, &error))
        {
            std::cerr << "bad UDP payload from " << address << ':'
                      << ntohs(peer.sin_port) << ": " << error << '\n';
            return;
        }

    std::ostringstream frame_message;
    frame_message << "udp from " << address << ':' << ntohs(peer.sin_port)
                  << " frame seq=" << header.seq
                  << " l2=" << header.l2_id
                  << " type="
                  << rk3506_tcp::message_type_name(header.msg_type)
                  << " payload_len=" << header.payload_len << '\n';
    append_log(log_path, frame_message.str());

    if (header.msg_type
        == static_cast<std::uint16_t>(rk3506_tcp::MessageType::SensorData))
        {
            rk3506_tcp::SensorDataSummary summary;
            if (rk3506_tcp::parse_sensor_data_binary(
                    header,
                    payload_begin,
                    header.payload_len,
                    &summary,
                    &error))
                {
                    append_log(log_path, sensor_summary_text(summary));
                    update_node_snapshot(state_path, summary);
                }
            else
                {
                    std::cerr << "  parse SENSOR_DATA failed: " << error
                              << '\n';
                }
        }
    else if (header.msg_type
             == static_cast<std::uint16_t>(rk3506_tcp::MessageType::NodeInfo))
        {
            rk3506_tcp::NodeInfoSummary summary;
            if (rk3506_tcp::parse_node_info_binary(
                    header,
                    payload_begin,
                    header.payload_len,
                    &summary,
                    &error))
                {
                    update_node_info_snapshot(state_path, summary);
                    append_log(log_path,
                               "  NODE_INFO l2="
                                   + std::to_string(summary.l2_id)
                                   + " l3=" + std::to_string(summary.l3_id)
                                   + " sensor="
                                   + std::to_string(summary.sensor_id)
                                   + " type=" + summary.data_type + "\n");
                }
            else
                {
                    std::cerr << "  parse NODE_INFO failed: " << error
                              << '\n';
                }
        }
    else if (header.msg_type
                 == static_cast<std::uint16_t>(
                     rk3506_tcp::MessageType::L2Heartbeat)
             || header.msg_type
                    == static_cast<std::uint16_t>(
                        rk3506_tcp::MessageType::L2Hello))
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            touch_l2(header.l2_id);
            write_state_file_unlocked(state_path);
        }
    else
        {
            append_log(log_path, "  binary payload ignored\n");
        }
}

} // namespace

int main(int argc, char **argv)
{
    std::uint16_t port = 35060U;
    std::string log_path = "/tmp/rk3506_udp_received.log";
    std::string state_path = "/tmp/rk3506_udp_nodes.tsv";
    std::string delete_path = "/tmp/rk3506_udp_delete.tsv";

    for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "-h" || arg == "--help")
                {
                    print_usage(argv[0]);
                    return 0;
                }
            if (arg == "--port")
                {
                    if (++i >= argc || !parse_port(argv[i], &port))
                        {
                            std::cerr << "invalid --port value\n";
                            return 1;
                        }
                    continue;
                }
            if (arg == "--log-file")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --log-file value\n";
                            return 1;
                        }
                    log_path = argv[i];
                    continue;
                }
            if (arg == "--state-file")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --state-file value\n";
                            return 1;
                        }
                    state_path = argv[i];
                    continue;
                }
            if (arg == "--delete-file")
                {
                    if (++i >= argc)
                        {
                            std::cerr << "missing --delete-file value\n";
                            return 1;
                        }
                    delete_path = argv[i];
                    continue;
                }

            std::cerr << "unknown option: " << arg << '\n';
            print_usage(argv[0]);
            return 1;
        }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    {
        std::ofstream log(log_path, std::ios::trunc);
        if (!log.is_open())
            {
                std::cerr << "log file: cannot open " << log_path << '\n';
                return 1;
            }
        log << "RK3506 UDP receive log started\n";
    }
    {
        std::ofstream state(state_path, std::ios::trunc);
        if (!state.is_open())
            {
                std::cerr << "state file: cannot open " << state_path << '\n';
                return 1;
            }
    }

    const int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0)
        {
            std::cerr << "socket: " << std::strerror(errno) << '\n';
            return 1;
        }

    int reuse = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    timeval receive_timeout = {};
    receive_timeout.tv_sec = 0;
    receive_timeout.tv_usec = 500000;
    setsockopt(server_fd,
               SOL_SOCKET,
               SO_RCVTIMEO,
               &receive_timeout,
               sizeof(receive_timeout));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(server_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address))
        != 0)
        {
            std::cerr << "bind: " << std::strerror(errno) << '\n';
            close(server_fd);
            return 1;
        }

    std::thread state_thread(
        [&]
            {
                while (keep_running)
                    {
                        {
                            std::lock_guard<std::mutex> lock(state_mutex);
                            process_delete_file_unlocked(delete_path,
                                                         log_path);
                            write_state_file_unlocked(state_path);
                        }
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(500));
                    }
            });

    std::cout << "RK3506 UDP receiver listening on 0.0.0.0:" << port
              << '\n';
    append_log(log_path,
               "RK3506 UDP receiver listening on 0.0.0.0:"
                   + std::to_string(port) + "\n");

    std::vector<std::uint8_t> datagram(rk3506_tcp::kMaxDatagramLength);

    while (keep_running)
        {
            sockaddr_in peer = {};
            socklen_t peer_len = sizeof(peer);
            const auto received = recvfrom(
                server_fd,
                datagram.data(),
                datagram.size(),
                0,
                reinterpret_cast<sockaddr *>(&peer),
                &peer_len);
            if (received < 0)
                {
                    if (errno == EINTR)
                        {
                            continue;
                        }
                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            continue;
                        }
                    std::cerr << "recvfrom: " << std::strerror(errno)
                              << '\n';
                    break;
                }

            handle_datagram(
                datagram.data(),
                static_cast<std::size_t>(received),
                peer,
                log_path,
                state_path);
        }

    close(server_fd);
    keep_running = 0;
    if (state_thread.joinable())
        {
            state_thread.join();
        }
    return 0;
}
