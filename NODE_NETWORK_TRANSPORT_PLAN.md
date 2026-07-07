# RK3506 三级节点控制与传输方案

## 1. 背景

当前系统采用三级结构：

- 第一层：RK3506，作为上级控制器和集中监控主机。
- 第二层：星闪控制器，作为区域信号收集层和协议汇聚层。
- 第三层：星闪控制器，直接连接传感器，负责采集原始数据。

第三层采集传感器数据后，通过 SLE 星闪链路上传给第二层。第二层对第三层节点的数据进行汇聚、缓存、状态整理和必要预处理，再通过 UDP 上传到 RK3506。第一版只验证单个 L2 与单个 L3 的链路；多 L2、多 L3 属于后续扩展目标。

## 2. 总体目标

- RK3506 第一版负责状态展示和数据汇总；命令下发作为后续目标。
- 第二层星闪控制器第一版负责管理 1 个第三层星闪采集节点；多 L3 管理作为后续目标。
- 第三层星闪控制器负责传感器采集、基础校验和本地上报。
- 第二层与第三层之间使用 SLE 星闪通信，第一版参考 BearPi SLE Gateway 的透传思路。
- RK3506 与第二层之间使用 UDP 通信。
- TUI 不直接处理 UDP 或 SLE 细节，只展示 RK3506 汇总后的系统状态。

## 3. 推荐架构

```text
┌────────────────────────────────────────────────────────────┐
│ 第一层：RK3506 上级控制器                                   │
│                                                            │
│  ┌──────────────────┐    ┌──────────────────────────────┐  │
│  │ UDP Receiver      │ -> │ Node State Store              │  │
│  │ - recv datagrams  │    │ - L2 controller state         │  │
│  │ - parse frames    │    │ - L3 sensor node state        │  │
│  │ - update state    │    │ - latest node/data snapshots  │  │
│  └──────────────────┘    └───────────────┬──────────────┘  │
│                                           │                 │
│                           ┌───────────────▼──────────────┐  │
│                           │ FTXUI Dashboard               │  │
│                           │ - node list                   │  │
│                           │ - node detail                 │  │
│                           │ - receive log                 │  │
│                           └──────────────────────────────┘  │
└────────────────────────────────────────────────────────────┘
             ▲ UDP
             │
┌────────────┴────────────┐        ┌─────────────────────────┐
│ 第二层：星闪汇聚控制器 A │        │ 第二层：星闪汇聚控制器 B │
│ - manage L3 nodes        │        │ - manage L3 nodes        │
│ - aggregate data         │        │ - aggregate data         │
│ - cache latest state     │        │ - cache latest state     │
│ - UDP sender to RK3506   │        │ - UDP sender to RK3506   │
└────────────┬────────────┘        └────────────┬────────────┘
             │ SLE 星闪                          │ SLE 星闪
     ┌───────┴────────┐                 ┌────────┴───────┐
     │                │                 │                │
┌────▼────┐      ┌────▼────┐       ┌────▼────┐      ┌────▼────┐
│ 第三层  │      │ 第三层  │       │ 第三层  │      │ 第三层  │
│ 采集节点 │      │ 采集节点 │       │ 采集节点 │      │ 采集节点 │
│ Sensor  │      │ Sensor  │       │ Sensor  │      │ Sensor  │
└─────────┘      └─────────┘       └─────────┘      └─────────┘
```

上图展示最终可扩展形态。第一版验证范围只实现一条链路：

```text
1 个 L3 采集节点 -> 1 个 L2 汇聚控制器 -> 1 个 RK3506 UDP 接收端
```

## 4. 分层职责

第一版验证范围先收敛为 `1 个 L2 汇聚控制器 + 1 个 L3 采集节点 + 1 个 RK3506 UDP 接收端`。多 L3、多 L2、命令下发、补报缓存等能力作为后续扩展，不进入第一版闭环。

### 4.1 RK3506

- 监听 UDP 端口，第一版接收单个第二层星闪汇聚控制器上报的数据报。
- 维护第二层控制器在线状态。
- 维护第三层传感器节点的间接状态。
- 接收第二层上传的 `NODE_INFO` 和 `SENSOR_DATA`。
- 第一版不向第二层下发控制命令；命令下发作为后续扩展。
- 为 TUI 提供统一状态快照。

建议端口：

```text
UDP 35060: RK3506 <- 第二层星闪汇聚控制器
```

### 4.2 第二层星闪汇聚控制器

- 作为 UDP sender 主动向 RK3506 上报数据。
- 作为 SLE 星闪网络中的网关或上级节点，第一版管理单个第三层采集节点。
- 接收第三层传感器数据、状态和告警。
- 对第三层数据进行汇总、去重、缓存和必要预处理。
- 向 RK3506 上传 L2 心跳、节点信息和传感器数据。
- 第一版不处理 RK3506 下发命令。
- UDP 无连接；当 RK3506 暂不可达时，第一版只继续发送最新数据，不实现补报缓存。

### 4.3 第三层星闪采集控制器

- 连接具体传感器，例如电机、IMU、雷达、温度、电流、电压等。
- 完成传感器采样、基础校验、单位转换和简单状态判断。
- 通过 SLE 星闪协议向第二层上传数据。
- 第一版不接收第二层转发的配置或控制命令；配置下发作为后续目标。
- 第一版不要求第三层单独向 RK3506 上报心跳。

### 4.4 SLE 与 UDP 承载说明

BearPi SLE Gateway 示例中的关键链路是：一侧开发板通过 SLE 收发透传数据，网关侧收到 SLE 数据后使用 UDP `sendto` 转发到 UDP 服务端。当前方案沿用这个方向：

- L3 与 L2 之间只关心 SLE 透传出来的业务 payload。
- L2 对来自 L3 的 payload 做汇聚，把 `l2_id` 写入 UDP `FrameHeader`，把 `l3_id/sensor_id` 保留在业务 payload 中，封装成既有应用层数据协议。
- L2 到 RK3506 使用 UDP 数据报承载完整应用层帧，第一版采用二进制 `FrameHeader + binary payload`，不使用 JSON 作为正式传输格式。
- 单个 UDP 数据报必须包含一个完整 `FrameHeader + payload`，不跨 UDP 数据报拆分。
- 单个 UDP 数据报总长度必须控制在 1200 字节以内，包含 `FrameHeader` 和 payload。
- 如后续 payload 超过局域网 MTU，应在应用层新增分片消息或摘要消息，不依赖 IP 分片。

### 4.5 第一版链路方向

第一版只实现单向上报链路：

```text
L3 采集节点 -> SLE 透传 -> L2 汇聚控制器 -> UDP -> RK3506
```

`COMMAND` 和 `COMMAND_ACK` 在协议枚举中保留，但第一版不实现 RK3506 到 L2/L3 的命令下发。L2 侧 UDP socket 第一版只需要发送，不要求监听 RK3506 下行命令。

### 4.6 SLE 透传 payload 定义

第一版 L3 到 L2 的 SLE 透传 payload 使用轻量二进制格式，L3 不直接生成完整 UDP 协议帧。L2 收到 SLE payload 后，先校验 SLE 帧，再补充 `l2_id`、UDP 帧头、序号、转发时间戳和 `payload_crc32`，最后发送给 RK3506。

SLE payload 固定头：

```text
SleSensorFrame
  magic               uint16   固定魔数，建议 0x5331，表示 SLE v1
  version             uint8    SLE payload 版本，初版为 1
  msg_type            uint8    SLE 消息类型，第一版使用 0x01 表示 SENSOR_DATA
  l3_id               uint32   第三层采集节点 ID
  sensor_id           uint32   传感器 ID，同一个 l3_id 下唯一
  data_type           uint16   数据类型，沿用 UDP 协议中的 data_type 枚举
  data_format         uint16   数值格式，第一版建议 FLOAT32
  sample_count        uint16   每个数组包含的采样点数量
  array_count         uint16   当前 payload 携带的数组数量
  sample_timestamp_us uint64   L3 采样时间戳，单位 us；无法提供时填 0
  payload_len         uint16   后续数组区域字节数
  payload_crc32       uint32   arrays 区域 CRC32
  arrays              bytes    repeated SleDataArray
```

SLE 数组区域：

```text
SleDataArray
  array_id     uint16
  unit         uint16
  value_count  uint16
  reserved     uint16
  values       repeated number，格式由 data_format 决定
```

SLE 透传规则：

- 第一版只要求 `SENSOR_DATA`，`NODE_INFO` 可由 L2 侧静态配置或后续补充。
- L3 不携带 `l2_id`，`l2_id` 由 L2 在 UDP `FrameHeader` 中填入。
- SLE payload 与 UDP payload 都采用二进制格式。
- SLE `payload_crc32` 使用与 UDP `payload_crc32` 相同的 CRC-32/IEEE 802.3 参数。
- L2 必须校验 SLE `magic`、`version`、`payload_len` 和 `payload_crc32`；校验失败的 SLE 帧直接丢弃，不转发到 RK3506。
- SLE 单帧也应尽量小于 1200 字节；如 SLE 实际 MTU 更小，应由 L3 降低 `sample_count` 或减少数组数量。
- 第一版不做 SLE 应用层分片。

## 5. 第一版消息类型

第一版先精简为以下消息，其中 `COMMAND`、`COMMAND_ACK`、`ERROR` 作为协议预留，第一版核心链路只实现 `L2_HELLO`、`L2_HEARTBEAT`、`NODE_INFO`、`SENSOR_DATA`：

```text
L2_HELLO        第二层控制器注册
L2_HEARTBEAT    第二层控制器心跳
NODE_INFO       第三层传感器节点信息
SENSOR_DATA     第三层传感器数据
COMMAND         RK3506 下发命令
COMMAND_ACK     第二层或第三层命令响应
ERROR           协议或业务错误
```

暂不实现或不作为第一版核心消息：

```text
L3_JOIN
L3_LEAVE
L3_HEARTBEAT
L3_STATUS
L3_METRICS
SENSOR_SUMMARY
SENSOR_CONFIG
LOG
EVENT
```

## 6. 心跳与离线规则

第一版只要求 L2 向 RK3506 发送心跳，L3 不单独向 RK3506 发送心跳。

规则：

1. L2 启动后发送 `L2_HELLO`。
2. L2 每 1 秒发送 `L2_HEARTBEAT`。
3. RK3506 如果 3 秒没有收到某个 L2 的心跳，则将该 L2 标记为 `Offline`。
4. L2 恢复心跳后，该 L2 标记为 `Online`。
5. L2 下所有未删除的 L3 传感器节点跟随父级显示状态。
6. L3 传感器节点不因为超时自动删除。

显示建议：

```text
[ON]  L2-1
[ON]  L2-1/L3-11/S1 IMU
[OFF] L2-2
[OFF] L2-2/L3-21/S3 CURRENT
```

## 7. NODE_INFO 与 SENSOR_DATA 对应关系

`NODE_INFO` 和 `SENSOR_DATA` 必须通过同一个三元组一一对应，其中 `l2_id` 来自 UDP `FrameHeader`，`l3_id` 和 `sensor_id` 来自业务 payload：

```text
(l2_id, l3_id, sensor_id)
```

含义：

- 一个有效传感器节点最多保留一份最新 `NODE_INFO`。
- 一个有效传感器节点最多保留一份最新 `SENSOR_DATA`。
- `NODE_INFO` 描述“这个节点是谁、数据结构是什么”。
- `SENSOR_DATA` 描述“这个节点当前测到了什么”。
- 有多少个有效 `NODE_INFO`，系统最终应该能找到多少个对应 `SENSOR_DATA`。
- 允许先收到 `SENSOR_DATA`，此时 RK3506 隐式创建临时节点，后续由 `NODE_INFO` 补全。
- 允许先收到 `NODE_INFO`，此时节点显示为已注册但暂无数据。

建议状态：

```text
InfoOnly      已收到 NODE_INFO，尚无 SENSOR_DATA
DataOnly      已收到 SENSOR_DATA，尚无 NODE_INFO
Ready         NODE_INFO 与 SENSOR_DATA 均已收到
Offline       所属 L2 心跳超时
```

## 8. 节点标识

系统需要同时识别第二层汇聚控制器和第三层传感器节点：

```text
l2_id       第二层星闪汇聚控制器 ID
l3_id       第三层星闪采集节点 ID，在所属 l2_id 下唯一
sensor_id   传感器 ID，在所属 l3_id 下唯一
global_id   可由 l2_id + l3_id + sensor_id 组合得到
```

示例：

```text
L2-1
L2-1/L3-11/S1 IMU
L2-1/L3-11/S4 CURRENT
L2-2/L3-21/S3 TEMPERATURE
```

## 9. 状态模型建议

```text
L2ControllerState
  l2_id
  name
  ip
  online
  last_heartbeat_time
  udp_peer
  rx_bytes
  rx_packets
  tx_bytes
  tx_packets

SensorNodeState
  l2_id
  l3_id
  sensor_id
  global_id
  name
  type
  node_info
  sensor_data
  info_received
  data_received
  last_data_time
  deleted
```

传感器节点显示状态由两部分决定：

- 所属 L2 是否在线。
- 当前节点是否已删除、是否已有 `NODE_INFO`、是否已有 `SENSOR_DATA`。

## 10. 删除机制

第一版删除只发生在 RK3506/TUI 本地，不要求通知 L2。

规则：

- TUI 选中节点后按 `d` 删除。
- 删除 L3 传感器节点时，只从 RK3506 本地状态移除该节点。
- 删除 L2 控制器时，同时移除该 L2 及其下所有传感器节点。
- 第一版不维护 ignore list；删除后如果再次收到该节点的 `NODE_INFO` 或 `SENSOR_DATA`，节点重新出现。

## 11. 与当前 TUI 的关系

当前 `helloworld` 可以继续作为展示层。后续建议演进为：

```text
UDP Receiver -> Node State Store -> NodePanel/TUI
```

TUI 行为建议：

- 左侧显示 L2 控制器和 L3 传感器节点。
- 右侧显示当前选中节点详情。
- `Receive Log` 显示最近接收到的消息。
- `/` 搜索节点。
- `d` 删除当前选中节点。
- `q` 或 `Esc` 退出；如果在搜索模式中，`Esc` 优先退出搜索。

## 12. 初期落地计划

### 阶段 1：消息类型收敛

- 代码中保留 `L2_HELLO`、`L2_HEARTBEAT`、`NODE_INFO`、`SENSOR_DATA`、`COMMAND`、`COMMAND_ACK`、`ERROR`。
- 旧的 `L3_JOIN/L3_STATUS/SENSOR_CONFIG` 等先不再作为核心路径。

### 阶段 2：L2 心跳

- 第二层每 1 秒发送 `L2_HEARTBEAT`。
- RK3506 维护 `last_heartbeat_time`。
- 超过 3 秒未更新则显示 `Offline`。

### 阶段 3：节点信息与数据对应

- 使用 `(l2_id, l3_id, sensor_id)` 作为节点 key。
- `NODE_INFO` 更新节点元数据。
- `SENSOR_DATA` 更新节点最新数据。
- 允许任意一个先到，并在 TUI 中显示 `InfoOnly/DataOnly/Ready`。

### 阶段 4：删除

- TUI 增加按 `d` 删除当前节点。
- 删除后再次收到上报则重新创建。

## 13. 风险与注意事项

- 第二层是汇聚层，单个第二层故障会影响其下所有第三层节点显示。
- 第一版 L3 不单独上报心跳，因此 L3 在线/离线状态跟随所属 L2。
- 如果 L2 在线但某个 L3 传感器长期没有数据，第一版不自动删除；后续可增加 `Stale` 状态。
- `NODE_INFO` 与 `SENSOR_DATA` 的 key 必须稳定，否则 TUI 会误认为是新节点。
- 删除不维护 ignore list 时，节点可能因为再次上报而重新出现，这是第一版预期行为。

## 14. 当前结论

当前方案采用三级控制架构：

- RK3506 作为第一层上级控制器，第一版通过 UDP 接收单个第二层星闪汇聚控制器上报；多 L2 接入作为后续目标。
- 第二层星闪控制器第一版负责信号收集、单个第三层管理和数据汇聚；命令转发作为后续目标。
- 第三层星闪控制器直接连接传感器，采集数据后通过 SLE 星闪上传到第二层。
- 第一版消息类型精简为 `L2_HELLO`、`L2_HEARTBEAT`、`NODE_INFO`、`SENSOR_DATA`、`COMMAND`、`COMMAND_ACK`、`ERROR`。
- `NODE_INFO` 和 `SENSOR_DATA` 通过 `(l2_id, l3_id, sensor_id)` 一一对应。
- L2 心跳承担链路在线状态，L3 节点生命周期由信息/数据上报和手动删除决定。
