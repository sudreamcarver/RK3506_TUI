# MCU Serial

Small Linux serial/UART receiving library for MCU communication, with a command-line receiver example.

## Build

```sh
cmake -S . -B build
cmake --build build
```

The build creates:

```text
build/libmcu_serial.a
build/mcu_serial_receiver
```

## C/C++ API

Public header:

```c
#include "mcu_serial.h"
```

Main functions:

```c
void mcu_serial_default_config(mcu_serial_config_t *config);
int mcu_serial_open(mcu_serial_t *serial, const mcu_serial_config_t *config);
ssize_t mcu_serial_receive(mcu_serial_t *serial, void *buffer, size_t length);
void mcu_serial_close(mcu_serial_t *serial);
const char *mcu_serial_strerror(int error_code);
```

`mcu_serial_receive()` returns:

```text
> 0   number of bytes received
0     timeout, no data received
-1    error, check errno
```

### C++ Example

See `examples/cpp_receive.cpp`.

Compile it manually:

```sh
c++ -std=c++17 -Iinclude examples/cpp_receive.cpp build/libmcu_serial.a -o build/cpp_receive
```

If you use CMake in another project:

```cmake
target_include_directories(your_app PRIVATE /path/to/serial/include)
target_link_libraries(your_app PRIVATE /path/to/serial/build/libmcu_serial.a)
```

## Run

Example for a USB serial adapter:

```sh
./build/mcu_serial_receiver -d /dev/ttyUSB0 -b 115200
```

Example for a board UART:

```sh
./build/mcu_serial_receiver -d /dev/ttyS1 -b 115200
```

Print bytes as hex:

```sh
./build/mcu_serial_receiver -d /dev/ttyUSB0 -b 115200 --hex
```

Save received bytes to a file while also printing them:

```sh
./build/mcu_serial_receiver -d /dev/ttyUSB0 -b 115200 -o capture.bin
```

## Options

```text
-d, --device PATH      Serial device path. Default: /dev/ttyUSB0
-b, --baud RATE       Baud rate. Default: 115200
--data-bits N         Data bits: 5, 6, 7, or 8. Default: 8
--stop-bits N         Stop bits: 1 or 2. Default: 1
--parity MODE         Parity: none, even, or odd. Default: none
-t, --timeout MS      Read timeout in milliseconds. Default: 100
-o, --output PATH     Append received raw bytes to file
--hex                 Print received bytes as hex instead of text
-h, --help            Show help
```

## Permissions

If the device cannot be opened, add your user to the serial device group and log in again:

```sh
sudo usermod -aG dialout "$USER"
```

On some systems the group is `tty` or `uucp` instead of `dialout`.
