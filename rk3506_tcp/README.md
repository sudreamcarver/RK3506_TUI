# RK3506 UDP Receiver

RK3506 端 UDP 接收与解析示例，用于接收第二层星闪汇聚控制器上传的数据。

## Build

```sh
cmake -S rk3506_tcp -B rk3506_tcp/build
cmake --build rk3506_tcp/build
```

## Run

```sh
./rk3506_tcp/build/rk3506_tcp_receiver --port 35060
```

默认监听 `0.0.0.0:35060`。

## Current Scope

- 接收 UDP datagram
- 每个 datagram 承载一个完整二进制 `FrameHeader + payload`
- 校验 magic、version、header size、payload length、payload CRC32
- 解析 `NODE_INFO`、`SENSOR_DATA`、`L2_HELLO`、`L2_HEARTBEAT`
- 打印 `l2_id`、`l3_id`、`sensor_id`、`data_type`、`array_count` 和数组摘要

当前 payload 使用文档定义的二进制格式，`l2_id` 只来自 `FrameHeader`。
