#ifndef MCU_SERIAL_H
#define MCU_SERIAL_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 默认串口参数。
 *
 * 外部程序可以先调用 mcu_serial_default_config() 填充默认值，
 * 然后只覆盖自己关心的字段，例如 device 和 baud。
 */
#define MCU_SERIAL_DEFAULT_DEVICE "/dev/ttyUSB0"
#define MCU_SERIAL_DEFAULT_BAUD 115200
#define MCU_SERIAL_DEFAULT_TIMEOUT_MS 100

/*
 * 校验位配置。
 *
 * MCU 和主机两端必须使用一致的校验位，否则会出现接收数据错误、
 * 丢字节或帧错误。
 */
typedef enum {
    MCU_SERIAL_PARITY_NONE = 0,
    MCU_SERIAL_PARITY_EVEN,
    MCU_SERIAL_PARITY_ODD
} mcu_serial_parity_t;

/*
 * 串口配置结构体。
 *
 * device:
 *   Linux 串口设备路径，例如 /dev/ttyUSB0、/dev/ttyS1、/dev/ttyAMA0。
 *
 * baud:
 *   波特率，例如 9600、115200、921600。必须是当前系统 termios 支持的值。
 *
 * data_bits:
 *   数据位，支持 5、6、7、8。常见 MCU 通信通常使用 8。
 *
 * stop_bits:
 *   停止位，支持 1 或 2。常见配置通常是 1。
 *
 * parity:
 *   校验位，见 mcu_serial_parity_t。
 *
 * timeout_ms:
 *   单次接收超时时间。mcu_serial_receive() 在超时无数据时返回 0，
 *   这样调用方可以在自己的主循环中继续处理其他任务。
 */
typedef struct {
    const char *device;
    int baud;
    int data_bits;
    int stop_bits;
    mcu_serial_parity_t parity;
    int timeout_ms;
} mcu_serial_config_t;

/*
 * 串口句柄。
 *
 * 调用方只需要把它当作不透明句柄使用：
 *   1. 传给 mcu_serial_open()
 *   2. 传给 mcu_serial_receive()
 *   3. 最后传给 mcu_serial_close()
 *
 * fd 字段暴露出来是为了保持接口简单，但业务代码不应该直接修改它。
 */
typedef struct {
    int fd;
} mcu_serial_t;

/*
 * 填充默认配置。
 *
 * 参数:
 *   config - 要初始化的配置结构体指针。传 NULL 时函数直接返回。
 */
void mcu_serial_default_config(mcu_serial_config_t *config);

/*
 * 打开并配置串口。
 *
 * 参数:
 *   serial - 输出串口句柄，成功后可用于接收数据。
 *   config - 串口配置。传 NULL 时使用默认配置。
 *
 * 返回:
 *   0  - 成功
 *   -1 - 失败，调用方可读取 errno，或用 mcu_serial_strerror(errno) 转成文本。
 */
int mcu_serial_open(mcu_serial_t *serial, const mcu_serial_config_t *config);

/*
 * 从串口接收一段数据。
 *
 * 这个函数是“单次读取”，不会自己循环接收完整协议帧。上层程序应根据
 * 自己的 MCU 协议决定如何拼包、校验、解析。
 *
 * 返回:
 *   >0 - 实际收到的字节数
 *   0  - 在 timeout_ms 内没有收到数据
 *   -1 - 读取失败，调用方可检查 errno
 */
ssize_t mcu_serial_receive(mcu_serial_t *serial, void *buffer, size_t length);

/*
 * 关闭串口。
 *
 * 可以重复调用；如果 serial 为空或串口未打开，函数不会做任何操作。
 */
void mcu_serial_close(mcu_serial_t *serial);

/*
 * 将 errno 错误码转换为可读文本。
 *
 * 示例:
 *   fprintf(stderr, "%s\n", mcu_serial_strerror(errno));
 */
const char *mcu_serial_strerror(int error_code);

#ifdef __cplusplus
}
#endif

#endif
