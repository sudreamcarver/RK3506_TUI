#include "mcu_serial.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READ_BUFFER_SIZE 512

/* 信号处理函数只修改 sig_atomic_t 标志，主循环看到后再安全退出。 */
static volatile sig_atomic_t keep_running = 1;

/* CLI 自己的配置：串口配置 + 命令行示例额外需要的输出选项。 */
typedef struct {
    mcu_serial_config_t serial;
    const char *output_path;
    bool hex;
} app_config_t;

static void handle_signal(int signo)
{
    (void)signo;
    keep_running = 0;
}

static void print_usage(const char *program)
{
    printf(
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  -d, --device PATH      Serial device path. Default: " MCU_SERIAL_DEFAULT_DEVICE "\n"
        "  -b, --baud RATE       Baud rate. Default: %d\n"
        "      --data-bits N     Data bits: 5, 6, 7, or 8. Default: 8\n"
        "      --stop-bits N     Stop bits: 1 or 2. Default: 1\n"
        "      --parity MODE     Parity: none, even, or odd. Default: none\n"
        "  -t, --timeout MS      Read timeout in milliseconds. Default: %d\n"
        "  -o, --output PATH     Append received raw bytes to file\n"
        "      --hex             Print received bytes as hex instead of text\n"
        "  -h, --help            Show this help\n",
        program,
        MCU_SERIAL_DEFAULT_BAUD,
        MCU_SERIAL_DEFAULT_TIMEOUT_MS);
}

static bool parse_int(const char *value, int *out)
{
    char *end = NULL;
    long parsed;

    errno = 0;
    parsed = strtol(value, &end, 10);
    /*
     * end 检查可以拒绝 "115200abc" 这种部分合法字符串。
     * 上限只是防止明显异常的命令行输入进入后续配置逻辑。
     */
    if (errno != 0 || end == value || *end != '\0' || parsed < 0 || parsed > 10000000) {
        return false;
    }

    *out = (int)parsed;
    return true;
}

static int parse_args(int argc, char **argv, app_config_t *config)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing value for --device\n");
                return -1;
            }
            config->serial.device = argv[i];
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baud") == 0) {
            if (++i >= argc || !parse_int(argv[i], &config->serial.baud)) {
                fprintf(stderr, "Invalid value for --baud\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--data-bits") == 0) {
            if (++i >= argc || !parse_int(argv[i], &config->serial.data_bits)) {
                fprintf(stderr, "Invalid value for --data-bits\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--stop-bits") == 0) {
            if (++i >= argc || !parse_int(argv[i], &config->serial.stop_bits)) {
                fprintf(stderr, "Invalid value for --stop-bits\n");
                return -1;
            }
        } else if (strcmp(argv[i], "--parity") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing value for --parity\n");
                return -1;
            }
            if (strcmp(argv[i], "none") == 0) {
                config->serial.parity = MCU_SERIAL_PARITY_NONE;
            } else if (strcmp(argv[i], "even") == 0) {
                config->serial.parity = MCU_SERIAL_PARITY_EVEN;
            } else if (strcmp(argv[i], "odd") == 0) {
                config->serial.parity = MCU_SERIAL_PARITY_ODD;
            } else {
                fprintf(stderr, "Invalid parity: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (++i >= argc || !parse_int(argv[i], &config->serial.timeout_ms)) {
                fprintf(stderr, "Invalid value for --timeout\n");
                return -1;
            }
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (++i >= argc) {
                fprintf(stderr, "Missing value for --output\n");
                return -1;
            }
            config->output_path = argv[i];
        } else if (strcmp(argv[i], "--hex") == 0) {
            config->hex = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return -1;
        }
    }

    return 0;
}

static void print_hex(const unsigned char *buffer, ssize_t length)
{
    /* 每 16 字节换行，方便观察 MCU 发来的二进制内容。 */
    for (ssize_t i = 0; i < length; i++) {
        printf("%02X", buffer[i]);
        putchar((i + 1) % 16 == 0 ? '\n' : ' ');
    }
    if (length % 16 != 0) {
        putchar('\n');
    }
}

int main(int argc, char **argv)
{
    app_config_t config = {
        .output_path = NULL,
        .hex = false,
    };
    unsigned char buffer[READ_BUFFER_SIZE];
    mcu_serial_t serial;
    FILE *output = NULL;
    int parsed;

    mcu_serial_default_config(&config.serial);

    /*
     * 命令行程序只是库的使用示例。
     * 真正集成到 C++ 程序时，可以直接构造 mcu_serial_config_t，
     * 然后调用 mcu_serial_open()/mcu_serial_receive()/mcu_serial_close()。
     */
    parsed = parse_args(argc, argv, &config);
    if (parsed > 0) {
        return EXIT_SUCCESS;
    }
    if (parsed < 0) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (mcu_serial_open(&serial, &config.serial) != 0) {
        fprintf(stderr, "%s: %s\n", config.serial.device, mcu_serial_strerror(errno));
        return EXIT_FAILURE;
    }

    if (config.output_path != NULL) {
        output = fopen(config.output_path, "ab");
        if (output == NULL) {
            fprintf(stderr, "%s: %s\n", config.output_path, mcu_serial_strerror(errno));
            mcu_serial_close(&serial);
            return EXIT_FAILURE;
        }
    }

    fprintf(stderr, "Receiving from %s at %d baud. Press Ctrl+C to stop.\n",
            config.serial.device,
            config.serial.baud);

    while (keep_running) {
        ssize_t received = mcu_serial_receive(&serial, buffer, sizeof(buffer));

        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            fprintf(stderr, "read: %s\n", mcu_serial_strerror(errno));
            break;
        }

        if (received == 0) {
            /* 超时无数据不是错误，继续等待下一批 MCU 数据。 */
            continue;
        }

        if (output != NULL) {
            if (fwrite(buffer, 1, (size_t)received, output) != (size_t)received) {
                fprintf(stderr, "fwrite: %s\n", mcu_serial_strerror(errno));
                break;
            }
            fflush(output);
        }

        if (config.hex) {
            print_hex(buffer, received);
        } else {
            /* 文本模式直接原样输出收到的字节，适合 MCU 发送日志字符串的场景。 */
            fwrite(buffer, 1, (size_t)received, stdout);
            fflush(stdout);
        }
    }

    if (output != NULL) {
        fclose(output);
    }
    mcu_serial_close(&serial);
    fputc('\n', stderr);
    return EXIT_SUCCESS;
}
