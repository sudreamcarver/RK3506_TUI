# UDP Test Sender

宿主机测试发送程序，用来模拟第二层星闪汇聚控制器向 RK3506 上传数据。

发送端使用二进制 `FrameHeader + payload`，并自动填充 `FrameHeader.l2_id` 和 payload CRC32。

## Build

```sh
cmake -S tcp_test -B tcp_test/build
cmake --build tcp_test/build
```

## Run

先启动 RK3506 UDP 接收端：

```sh
./rk3506_tcp/build/rk3506_tcp_receiver --port 35060
```

再启动测试发送端：

```sh
./tcp_test/build/tcp_sensor_sender --host 127.0.0.1 --port 35060 --type imu
```

可选类型：

```text
voltage
current
temperature
imu
motor
battery
```

可选消息：

```text
sensor-data
node-info
l2-hello
l2-heartbeat
```

示例：发送 L2 启动消息：

```sh
./tcp_test/build/tcp_sensor_sender --host 127.0.0.1 --port 35060 --message l2-hello --l2-id 1
```

示例：发送 L2 心跳：

```sh
./tcp_test/build/tcp_sensor_sender --host 127.0.0.1 --port 35060 --message l2-heartbeat --l2-id 1
```

示例：发送 NODE_INFO：

```sh
./tcp_test/build/tcp_sensor_sender --host 127.0.0.1 --port 35060 --message node-info --type imu --sensor-id 1
```

发送 10 次，每次间隔 500 ms：

```sh
./tcp_test/build/tcp_sensor_sender --type imu --count 10 --interval-ms 500
```
