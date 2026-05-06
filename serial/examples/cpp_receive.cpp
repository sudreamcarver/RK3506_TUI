#include "mcu_serial.h"

#include <cerrno>
#include <cstdint>
#include <iostream>

int main()
{
    /*
     * mcu_serial_config_t 是 C 接口结构体，C++ 可以直接使用。
     * 建议先调用 mcu_serial_default_config()，再覆盖自己需要的字段，
     * 这样以后库里增加默认配置时，调用方不容易漏填。
     */
    mcu_serial_config_t config;
    mcu_serial_t serial;
    uint8_t buffer[256];

    mcu_serial_default_config(&config);
    config.device = "/dev/ttyUSB0";
    config.baud = 115200;

    /*
     * 打开失败时返回 -1，并设置 errno。
     * 常见失败原因包括设备不存在、权限不足、波特率不支持。
     */
    if (mcu_serial_open(&serial, &config) != 0) {
        std::cerr << "open failed: " << mcu_serial_strerror(errno) << '\n';
        return 1;
    }

    /*
     * 这里只演示单次读取。
     * 实际项目通常会把 mcu_serial_receive() 放在循环、线程或事件处理函数里，
     * 然后根据 MCU 协议解析 buffer 中的原始字节。
     */
    ssize_t received = mcu_serial_receive(&serial, buffer, sizeof(buffer));
    if (received > 0) {
        std::cout << "received " << received << " bytes\n";
    } else if (received == 0) {
        std::cout << "timeout, no data\n";
    } else {
        std::cerr << "receive failed: " << mcu_serial_strerror(errno) << '\n';
    }

    mcu_serial_close(&serial);
    return 0;
}
