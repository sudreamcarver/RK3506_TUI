#define _DEFAULT_SOURCE

#include "mcu_serial.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/*
 * 将用户传入的整数波特率转换为 termios 使用的 speed_t 常量。
 *
 * termios 不直接接受整数 115200，而是要求使用 B115200 这样的宏。
 * 不同 Linux/ libc 支持的高速波特率不完全一样，所以高速项用 #ifdef
 * 包起来；如果当前平台没有定义某个宏，就不会编译对应 case。
 */
static speed_t baud_to_speed(int baud)
{
    switch (baud) {
    case 50: return B50;
    case 75: return B75;
    case 110: return B110;
    case 134: return B134;
    case 150: return B150;
    case 200: return B200;
    case 300: return B300;
    case 600: return B600;
    case 1200: return B1200;
    case 1800: return B1800;
    case 2400: return B2400;
    case 4800: return B4800;
    case 9600: return B9600;
    case 19200: return B19200;
    case 38400: return B38400;
#ifdef B57600
    case 57600: return B57600;
#endif
#ifdef B115200
    case 115200: return B115200;
#endif
#ifdef B230400
    case 230400: return B230400;
#endif
#ifdef B460800
    case 460800: return B460800;
#endif
#ifdef B500000
    case 500000: return B500000;
#endif
#ifdef B576000
    case 576000: return B576000;
#endif
#ifdef B921600
    case 921600: return B921600;
#endif
#ifdef B1000000
    case 1000000: return B1000000;
#endif
#ifdef B1500000
    case 1500000: return B1500000;
#endif
#ifdef B2000000
    case 2000000: return B2000000;
#endif
#ifdef B3000000
    case 3000000: return B3000000;
#endif
    default: return 0;
    }
}

/*
 * 根据配置修改底层 termios 参数。
 *
 * fd 必须是已经 open() 成功的串口文件描述符。这里不负责打开或关闭 fd，
 * 只负责把串口切到适合 MCU 二进制通信的 raw 模式，并设置波特率、
 * 数据位、停止位、校验位和读取超时。
 */
static int configure_serial(int fd, const mcu_serial_config_t *config)
{
    struct termios tty;
    speed_t speed = baud_to_speed(config->baud);
    int vtime;

    if (speed == 0) {
        errno = EINVAL;
        return -1;
    }

    if (tcgetattr(fd, &tty) != 0) {
        return -1;
    }

    /*
     * raw 模式会关闭行缓冲、回显、特殊字符处理、换行转换等终端行为。
     * MCU 串口通信通常要收发原始字节，不能让终端驱动修改数据。
     */
    cfmakeraw(&tty);

    /* 先清除已有数据位设置，再写入调用方指定的数据位。 */
    tty.c_cflag &= ~CSIZE;
    switch (config->data_bits) {
    case 5: tty.c_cflag |= CS5; break;
    case 6: tty.c_cflag |= CS6; break;
    case 7: tty.c_cflag |= CS7; break;
    case 8: tty.c_cflag |= CS8; break;
    default:
        errno = EINVAL;
        return -1;
    }

    /* 停止位必须和 MCU 端配置一致，常见配置是 1。 */
    if (config->stop_bits == 2) {
        tty.c_cflag |= CSTOPB;
    } else if (config->stop_bits == 1) {
        tty.c_cflag &= ~CSTOPB;
    } else {
        errno = EINVAL;
        return -1;
    }

    /* 校验位必须和 MCU 端配置一致，常见配置是 none。 */
    tty.c_cflag &= ~(PARENB | PARODD);
    if (config->parity == MCU_SERIAL_PARITY_EVEN) {
        tty.c_cflag |= PARENB;
    } else if (config->parity == MCU_SERIAL_PARITY_ODD) {
        tty.c_cflag |= PARENB | PARODD;
    } else if (config->parity != MCU_SERIAL_PARITY_NONE) {
        errno = EINVAL;
        return -1;
    }

    /*
     * CLOCAL: 忽略 modem 控制线，普通 UART/USB 转串口通常需要这样设置。
     * CREAD:  开启接收功能。
     */
    tty.c_cflag |= CLOCAL | CREAD;
#ifdef CRTSCTS
    /* 默认关闭硬件流控，避免没有接 RTS/CTS 的串口线时无法正常收发。 */
    tty.c_cflag &= ~CRTSCTS;
#endif

    /* 关闭软件流控，避免数据中的 0x11/0x13 被当作 XON/XOFF 控制字符。 */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    /*
     * VMIN = 0, VTIME > 0:
     *   read() 最多等待 VTIME 个 0.1 秒单位。
     *   如果期间收到数据，read() 返回收到的字节数。
     *   如果超时仍无数据，read() 返回 0。
     *
     * termios 的 VTIME 只能精确到 100ms，所以这里向上取整。
     */
    vtime = (config->timeout_ms + 99) / 100;
    if (vtime < 1) {
        vtime = 1;
    } else if (vtime > 255) {
        vtime = 255;
    }
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = (cc_t)vtime;

    /* 输入和输出波特率都设置为相同值。 */
    if (cfsetispeed(&tty, speed) != 0 || cfsetospeed(&tty, speed) != 0) {
        return -1;
    }

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        return -1;
    }

    /*
     * 清空打开串口前后遗留在驱动缓冲区里的旧数据。
     * 这样调用方开始接收时，拿到的是配置完成后的新数据。
     */
    return tcflush(fd, TCIOFLUSH);
}

void mcu_serial_default_config(mcu_serial_config_t *config)
{
    if (config == NULL) {
        return;
    }

    config->device = MCU_SERIAL_DEFAULT_DEVICE;
    config->baud = MCU_SERIAL_DEFAULT_BAUD;
    config->data_bits = 8;
    config->stop_bits = 1;
    config->parity = MCU_SERIAL_PARITY_NONE;
    config->timeout_ms = MCU_SERIAL_DEFAULT_TIMEOUT_MS;
}

int mcu_serial_open(mcu_serial_t *serial, const mcu_serial_config_t *config)
{
    mcu_serial_config_t default_config;
    const mcu_serial_config_t *active_config = config;
    int fd;

    if (serial == NULL) {
        errno = EINVAL;
        return -1;
    }

    /*
     * 先标记为未打开状态。即使后续 open/configure 失败，调用方再调用
     * mcu_serial_close() 也不会误关一个无效描述符。
     */
    serial->fd = -1;

    /* 允许调用方传 NULL 快速使用默认串口配置。 */
    if (active_config == NULL) {
        mcu_serial_default_config(&default_config);
        active_config = &default_config;
    }

    if (active_config->device == NULL) {
        errno = EINVAL;
        return -1;
    }

    /*
     * O_NOCTTY 表示不要把该串口设备变成当前进程的控制终端。
     * 对库代码来说这是更安全的默认行为。
     */
    fd = open(active_config->device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        return -1;
    }

    /*
     * 如果配置失败，必须关闭刚打开的 fd，并恢复 errno，
     * 这样调用方看到的是 configure_serial() 失败的真实原因。
     */
    if (configure_serial(fd, active_config) != 0) {
        int saved_errno = errno;
        close(fd);
        errno = saved_errno;
        return -1;
    }

    serial->fd = fd;
    return 0;
}

ssize_t mcu_serial_receive(mcu_serial_t *serial, void *buffer, size_t length)
{
    if (serial == NULL || serial->fd < 0 || buffer == NULL || length == 0) {
        errno = EINVAL;
        return -1;
    }

    /*
     * 这里故意只封装 read()，不做协议层处理。
     * MCU 协议可能是定长包、换行结束、帧头帧尾、CRC 校验等，
     * 这些应放在上层业务代码中实现。
     */
    return read(serial->fd, buffer, length);
}

void mcu_serial_close(mcu_serial_t *serial)
{
    if (serial == NULL || serial->fd < 0) {
        return;
    }

    close(serial->fd);
    serial->fd = -1;
}

const char *mcu_serial_strerror(int error_code)
{
    return strerror(error_code);
}
