# 第二层到 RK3506 的 UDP 数据协议设计

## 1. 目标

本文档定义第二层星闪汇聚控制器到 RK3506 的 UDP 数据协议。该协议用于把第三层星闪采集节点的数据、状态、日志和告警统一上传到 RK3506。

当前重点先定义第一版消息类型、`NODE_INFO` 与 `SENSOR_DATA` 的对应关系、数据类型体系和不同类型的数据包结构。后续 C/C++ 实现时，可以根据本文档落地为枚举、结构体、解析器和 TUI 状态模型。

## 2. 基本原则

- 第二层到 RK3506 使用 UDP。
- UDP 是无连接数据报协议，不保证可靠、有序和不重复；第一版用 L2 心跳和应用层序号辅助判断链路状态与丢包情况。
- 第二层负责汇聚第三层数据，RK3506 不直接解析星闪协议。
- `l2_id` 只出现在 `FrameHeader` 中；`NODE_INFO` 和 `SENSOR_DATA` payload 只携带 `l3_id` 和 `sensor_id`。
- `NODE_INFO` 和 `SENSOR_DATA` 通过 `(l2_id, l3_id, sensor_id)` 一一对应。
- 不同传感器类型使用统一识别方式，但 payload 根据 `data_type` 决定。
- 简单数据使用一个数组，例如电压、电流、温度。
- 复合数据使用多个数组，例如 IMU 包含加速度、角速度、磁力计、姿态角等。
- 大数据不直接塞进普通指标包，应单独设计分片或摘要包。
- 第一版验证拓扑为 `1 个 L2 + 1 个 L3 + 1 个 RK3506`，只实现 一个L3 到 一个L2 到 RK3506 的单向上报链路。
- 第一版正式传输格式采用二进制 `FrameHeader + binary payload`，不使用 JSON 作为正式 UDP payload。文档中的 JSON 仅作为字段含义示例。
- 单个 UDP datagram 总长度必须控制在 1200 字节以内，包含 `FrameHeader` 和 payload。
- 第一版不做 UDP 应用层分片，不依赖 IP 分片。
- `COMMAND` 和 `COMMAND_ACK` 仅保留消息 ID，第一版不实现 RK3506 下行命令。

## 3. UDP 数据报格式

每个 UDP 数据报承载一个完整的应用层消息，格式仍为固定 `FrameHeader` + payload。数据协议保持不变，变化只发生在传输层。

```text
FrameHeader
  magic        uint32   固定魔数，建议 0x524B3335
  version      uint16   协议版本，初版为 1
  msg_type     uint16   消息类型
  header_size  uint16   帧头长度
  flags        uint16   标志位，第一版发送端必须填 0，接收端收到非 0 应记录告警但可继续解析
  l2_id        uint32   第二层星闪汇聚控制器 ID
  seq          uint32   第二层侧递增序号
  timestamp_us uint64   第二层发送时间戳，单位 us
  payload_len  uint32   payload 字节数
  payload_crc32 uint32  payload CRC32；payload_len 为 0 时对空 payload 计算 CRC32
  payload      bytes    根据 msg_type 解析
```

第一版 `FrameHeader` 固定长度为 36 字节：

```text
uint32 magic
uint16 version
uint16 msg_type
uint16 header_size
uint16 flags
uint32 l2_id
uint32 seq
uint64 timestamp_us
uint32 payload_len
uint32 payload_crc32
```

编码规则：

- 所有整数和浮点数均使用 little-endian。
- `header_size` 第一版固定为 36。
- `flags` 第一版固定为 0，暂不定义具体 bit。
- `l2_id` 必须在 header 中填写，payload 中不再重复携带 `l2_id`。
- `payload_crc32` 使用标准 CRC-32/IEEE 802.3 多项式 `0x04C11DB7`，初值 `0xFFFFFFFF`，输出异或 `0xFFFFFFFF`。
- 第一版不增加 header CRC。
- datagram 总长度必须小于等于 1200 字节。

接收端流程：

1. 从 UDP socket 读取一个完整 datagram。
2. datagram 长度必须至少包含固定长度 `FrameHeader`。
3. 校验 datagram 总长度不超过 1200 字节。
4. 校验 `magic`、`version`、`header_size`、`flags`、`payload_len`。
5. 校验 `header_size + payload_len` 必须等于当前 datagram 长度。
6. 计算 payload CRC32，并与 `payload_crc32` 比较。
7. 从 datagram 中取出 payload。
8. 根据 `msg_type` 分发解析。

## 4. 消息类型

```text
0x0001 L2_HELLO        第二层控制器注册
0x0002 L2_HEARTBEAT    第二层控制器心跳
0x0200 NODE_INFO       第三层传感器节点信息
0x0201 SENSOR_DATA     第三层传感器数据

0x0401 COMMAND         RK3506 下发命令，第一版预留不实现
0x0402 COMMAND_ACK     命令响应，第一版预留不实现
0x04FF ERROR           错误
```

第一版暂不实现 `L3_JOIN`、`L3_LEAVE`、`L3_HEARTBEAT`、`L3_STATUS`、`SENSOR_SUMMARY`、`SENSOR_CONFIG`、`LOG`、`EVENT`。如后续需要，可以在不破坏现有消息 ID 的前提下扩展。

## 5. 心跳与节点生命周期

第一版只有 L2 发送心跳：

- L2 启动后发送 `L2_HELLO`。
- L2 每 1 秒发送 `L2_HEARTBEAT`。
- RK3506 超过 3 秒未收到某 L2 心跳，则该 L2 显示 `Offline`。
- 该 L2 下所有未删除的传感器节点跟随父级显示 `Offline`。
- L3 传感器节点不因为超时自动删除。
- TUI 可手动删除节点；删除后再次收到 `NODE_INFO` 或 `SENSOR_DATA` 时，第一版默认重新创建。

## 6. NODE_INFO 与 SENSOR_DATA 对应关系

`NODE_INFO` 与 `SENSOR_DATA` 通过同一个 key 一一对应，其中 `l2_id` 来自 `FrameHeader`，`l3_id` 和 `sensor_id` 来自 payload：

```text
key = (l2_id, l3_id, sensor_id)
```

含义：

- 一个 key 对应一个传感器节点。
- 一个传感器节点最多保留一份最新 `NODE_INFO`。
- 一个传感器节点最多保留一份最新 `SENSOR_DATA`。
- `NODE_INFO` 描述“这个节点是谁、数据结构是什么”。
- `SENSOR_DATA` 描述“这个节点当前测到了什么”。
- 允许 `SENSOR_DATA` 先到，RK3506 临时创建节点，后续由 `NODE_INFO` 补全。
- 允许 `NODE_INFO` 先到，此时节点显示为已注册但暂无数据。

推荐状态：

```text
InfoOnly      已收到 NODE_INFO，尚无 SENSOR_DATA
DataOnly      已收到 SENSOR_DATA，尚无 NODE_INFO
Ready         NODE_INFO 与 SENSOR_DATA 均已收到
Offline       所属 L2 心跳超时
```

## 7. L2_HELLO 与 L2_HEARTBEAT payload

第一版 `L2_HELLO` 和 `L2_HEARTBEAT` 可以使用空 payload，L2 身份由 `FrameHeader.l2_id` 提供。后续如需显示 L2 名称、固件版本、能力位，可扩展为二进制 payload。

```text
L2_HELLO payload:
  empty

L2_HEARTBEAT payload:
  empty
```

## 8. NODE_INFO payload

`NODE_INFO` 用于描述第三层传感器节点的静态或半静态信息。节点首次出现、配置变化或 L2 重连补报时发送。

第一版 `NODE_INFO` payload 使用二进制 TLV 友好格式：

```text
NodeInfoPayload
  l3_id              uint32   第三层星闪采集节点 ID
  sensor_id          uint32   传感器 ID，同一个 l3_id 下唯一
  data_type          uint16   数据类型
  array_count        uint16   数组数量
  sample_rate_hz     float32  采样率
  name_len           uint16   传感器显示名长度，单位字节
  name               bytes    UTF-8，不要求以 0 结尾
  array_descriptors  repeated ArrayDescriptor
```

`ArrayDescriptor`：

```text
ArrayDescriptor
  array_id    uint16   数组 ID
  unit        uint16   单位枚举
  name_len    uint16   数组名长度，单位字节
  name        bytes    UTF-8，不要求以 0 结尾，例如 accel_x
```

示例：

以下 JSON 仅用于说明 payload 字段含义，实际 UDP `l2_id` 来自 `FrameHeader.l2_id`，不在 payload 中重复出现。

```json
{
  "l3_id": 11,
  "sensor_id": 1,
  "name": "IMU-01",
  "data_type": "IMU",
  "sample_rate_hz": 100,
  "array_descriptors": [
    { "array_id": 0, "name": "accel_x", "unit": "METER_PER_SEC2" },
    { "array_id": 1, "name": "accel_y", "unit": "METER_PER_SEC2" },
    { "array_id": 2, "name": "accel_z", "unit": "METER_PER_SEC2" }
  ]
}
```

## 9. SENSOR_DATA 通用 payload

`SENSOR_DATA` payload 建议由一个通用数据头和若干数据数组组成。

```text
SensorDataHeader
  l3_id             uint32   第三层星闪采集节点 ID
  sensor_id         uint32   传感器 ID，同一个 l3_id 下唯一
  data_type         uint16   数据类型；如果已发送 NODE_INFO，可省略或保留用于校验
  data_format       uint16   数值格式
  sample_count      uint16   每个数组包含的采样点数量
  array_count       uint16   当前 data_type 包含的数组数量
  sample_timestamp  uint64   第三层采样时间戳，单位 us
  forward_timestamp uint64   第二层转发时间戳，单位 us
  quality           uint16   数据质量，0-1000；1000 表示最好
  reserved          uint16   保留
  arrays            repeated DataArray
```

`DataArray`：

```text
DataArray
  array_id    uint16   数组 ID，由 data_type 决定
  unit        uint16   单位枚举
  value_count uint16   数值数量
  reserved    uint16   保留
  values      repeated number
```

说明：

- `sample_count` 表示同一时间窗口内的采样点数量。
- `array_count` 表示该类型数据包含多少组数组。
- `SENSOR_DATA` 第一版只携带 `data_type`、`unit`、`array_id` 和数值；数组名称来自 `NODE_INFO` 或接收端内置类型表。
- 对标量类传感器，`array_count = 1`。
- 对 IMU、GPS、姿态等复合传感器，`array_count > 1`。
- `value_count` 一般等于 `sample_count`，但三轴向量可以用一个数组承载 `sample_count * 3` 个值，也可以拆成 x/y/z 三个数组。本文建议三轴数据拆成独立数组，解析更直接。

## 10. 数值格式

```text
0x0001 INT16
0x0002 UINT16
0x0003 INT32
0x0004 UINT32
0x0005 FLOAT32
0x0006 FLOAT64
0x0007 FIXED_S16_1000   int16，真实值 = raw / 1000
0x0008 FIXED_S32_1000   int32，真实值 = raw / 1000
```

建议初期统一使用 `FLOAT32`，方便调试。后续如需降低带宽，再切换为定点数。

## 11. 单位枚举

```text
0x0000 NONE
0x0001 VOLT              V
0x0002 AMPERE            A
0x0003 CELSIUS           degC
0x0004 PERCENT           %
0x0005 RPM               rpm
0x0006 METER             m
0x0007 MILLIMETER        mm
0x0008 METER_PER_SEC     m/s
0x0009 METER_PER_SEC2    m/s^2
0x000A DEGREE_PER_SEC    deg/s
0x000B DEGREE            deg
0x000C RADIAN            rad
0x000D PASCAL            Pa
0x000E HPA               hPa
0x000F LUX               lx
0x0010 GAUSS             gauss
0x0011 MICRO_TESLA       uT
0x0012 PPM               ppm
0x0013 BOOLEAN           0/1
0x0014 COUNT             count
0x0015 BYTE              byte
0x0016 HERTZ             Hz
0x0017 WATT              W
0x0018 WATT_HOUR         Wh
0x0019 NEWTON            N
```

## 12. SLE 透传 payload 定义

L3 到 L2 的 SLE 透传数据不直接使用完整 UDP `FrameHeader`。L3 只发送轻量二进制采样帧，L2 负责补充 `l2_id`、UDP header、序号、转发时间戳和 `payload_crc32`。

第一版只定义 `SleSensorFrame`，用于承载 L3 采样数据：

```text
SleSensorFrame
  magic               uint16   固定魔数，建议 0x5331，表示 SLE v1
  version             uint8    SLE payload 版本，初版为 1
  msg_type            uint8    SLE 消息类型，第一版使用 0x01 表示 SENSOR_DATA
  l3_id               uint32   第三层采集节点 ID
  sensor_id           uint32   传感器 ID，同一个 l3_id 下唯一
  data_type           uint16   数据类型，沿用本文档 data_type 枚举
  data_format         uint16   数值格式，第一版建议 FLOAT32
  sample_count        uint16   每个数组包含的采样点数量
  array_count         uint16   当前 payload 携带的数组数量
  sample_timestamp_us uint64   L3 采样时间戳，单位 us；无法提供时填 0
  payload_len         uint16   后续数组区域字节数
  payload_crc32       uint32   arrays 区域 CRC32
  arrays              bytes    repeated SleDataArray
```

```text
SleDataArray
  array_id     uint16
  unit         uint16
  value_count  uint16
  reserved     uint16
  values       repeated number，格式由 data_format 决定
```

SLE 规则：

- 所有整数和浮点数均使用 little-endian。
- 第一版只支持 `msg_type = 0x01` 的传感器数据上报。
- L3 不携带 `l2_id`。
- L2 收到 SLE 帧后转换为 UDP `SENSOR_DATA` payload。
- SLE `payload_crc32` 使用与 UDP `payload_crc32` 相同的 CRC-32/IEEE 802.3 参数。
- L2 必须先校验 SLE `magic`、`version`、`payload_len` 和 `payload_crc32`；校验失败的 SLE 帧直接丢弃，不转发到 RK3506。
- 第一版不做 SLE 应用层分片。
- 单个 SLE payload 也应控制在 1200 字节以内；如 SLE 实际链路 MTU 更小，L3 应减少 `sample_count` 或数组数量。

## 13. 数据类型总表

下面先定义可能出现的数据类型。后续可以按实际传感器裁剪，但枚举值尽量保持稳定。

```text
0x0001 VOLTAGE             电压
0x0002 CURRENT             电流
0x0003 POWER               功率
0x0004 ENERGY              能量/电量累计
0x0005 TEMPERATURE         温度
0x0006 HUMIDITY            湿度
0x0007 PRESSURE            气压/压力
0x0008 LIGHT               光照
0x0009 DISTANCE            距离/测距
0x000A SWITCH              开关量
0x000B DIGITAL_INPUT       数字输入
0x000C ANALOG_INPUT        模拟输入

0x0101 MOTOR               电机综合数据
0x0102 ENCODER             编码器
0x0103 SERVO               舵机/执行器
0x0104 BATTERY             电池
0x0105 POWER_RAIL          电源轨

0x0201 IMU                 IMU 综合数据
0x0202 ACCELEROMETER       加速度计
0x0203 GYROSCOPE           陀螺仪
0x0204 MAGNETOMETER        磁力计
0x0205 ATTITUDE            姿态角/四元数

0x0301 GNSS                GNSS/GPS
0x0302 BAROMETER           气压计
0x0303 COMPASS             电子罗盘

0x0401 RADAR_SUMMARY       雷达摘要
0x0402 LIDAR_SUMMARY       激光雷达摘要
0x0403 CAMERA_SUMMARY      相机摘要
0x0404 VISION_RESULT       视觉识别结果

0x0501 GAS                 气体浓度
0x0502 SMOKE               烟雾
0x0503 FLAME               火焰
0x0504 VIBRATION           振动
0x0505 STRAIN              应变
0x0506 FORCE               力

0x0F01 CUSTOM_SCALAR       自定义单数组数据
0x0F02 CUSTOM_VECTOR       自定义多数组数据
```

## 14. 各数据类型数组定义

### 14.1 VOLTAGE 电压

```text
data_type: VOLTAGE
array_count: 1

array_id 0: voltage_v
unit: VOLT
含义: 电压值
```

适用于单路电压或多采样点电压曲线。若一个传感器同时采多路电压，建议使用 `POWER_RAIL`。

### 14.2 CURRENT 电流

```text
data_type: CURRENT
array_count: 1

array_id 0: current_a
unit: AMPERE
含义: 电流值
```

### 14.3 POWER 功率

```text
data_type: POWER
array_count: 1

array_id 0: power_w
unit: WATT
含义: 功率值，单位 W
```

第一版已定义 `WATT` 单位枚举，功率数据应优先使用 `WATT`。

### 14.4 ENERGY 能量

```text
data_type: ENERGY
array_count: 1

array_id 0: energy_wh
unit: WATT_HOUR
含义: 累计能量，单位 Wh
```

### 14.5 TEMPERATURE 温度

```text
data_type: TEMPERATURE
array_count: 1

array_id 0: temperature_c
unit: CELSIUS
含义: 温度
```

### 14.6 HUMIDITY 湿度

```text
data_type: HUMIDITY
array_count: 1

array_id 0: humidity_percent
unit: PERCENT
含义: 相对湿度
```

### 14.7 PRESSURE 压力/气压

```text
data_type: PRESSURE
array_count: 1

array_id 0: pressure_pa
unit: PASCAL
含义: 压力或气压
```

### 14.8 DISTANCE 距离

```text
data_type: DISTANCE
array_count: 1

array_id 0: distance_m
unit: METER
含义: 距离
```

### 14.9 SWITCH 开关量

```text
data_type: SWITCH
array_count: 1

array_id 0: state
unit: BOOLEAN
含义: 0 表示关闭，1 表示打开
```

### 14.10 ANALOG_INPUT 模拟输入

```text
data_type: ANALOG_INPUT
array_count: 1

array_id 0: raw_or_scaled_value
unit: NONE
含义: 模拟量输入，可由 NODE_INFO 说明量程和单位
```

### 14.11 MOTOR 电机综合数据

```text
data_type: MOTOR
array_count: 5

array_id 0: rpm
unit: RPM

array_id 1: current_a
unit: AMPERE

array_id 2: voltage_v
unit: VOLT

array_id 3: temperature_c
unit: CELSIUS

array_id 4: load_percent
unit: PERCENT
```

### 14.12 ENCODER 编码器

```text
data_type: ENCODER
array_count: 3

array_id 0: position_count
unit: COUNT

array_id 1: velocity_rpm
unit: RPM

array_id 2: angle_deg
unit: DEGREE
```

### 14.13 BATTERY 电池

```text
data_type: BATTERY
array_count: 5

array_id 0: voltage_v
unit: VOLT

array_id 1: current_a
unit: AMPERE

array_id 2: soc_percent
unit: PERCENT

array_id 3: temperature_c
unit: CELSIUS

array_id 4: cycle_count
unit: COUNT
```

### 14.14 POWER_RAIL 电源轨

```text
data_type: POWER_RAIL
array_count: 4

array_id 0: voltage_v
unit: VOLT

array_id 1: current_a
unit: AMPERE

array_id 2: power_w
unit: WATT

array_id 3: enable_state
unit: BOOLEAN
```

### 14.15 IMU 综合数据

IMU 是复合传感器，应包含多个数组。建议把三轴数据拆成 x/y/z 独立数组，便于 TUI 和日志显示。

```text
data_type: IMU
array_count: 10

array_id 0: accel_x
unit: METER_PER_SEC2

array_id 1: accel_y
unit: METER_PER_SEC2

array_id 2: accel_z
unit: METER_PER_SEC2

array_id 3: gyro_x
unit: DEGREE_PER_SEC

array_id 4: gyro_y
unit: DEGREE_PER_SEC

array_id 5: gyro_z
unit: DEGREE_PER_SEC

array_id 6: mag_x
unit: MICRO_TESLA

array_id 7: mag_y
unit: MICRO_TESLA

array_id 8: mag_z
unit: MICRO_TESLA

array_id 9: temperature_c
unit: CELSIUS
```

如果 IMU 模块不带磁力计，对应数组可以不上传，但 `array_count` 必须等于实际上传数组数量。

### 14.16 ACCELEROMETER 加速度计

```text
data_type: ACCELEROMETER
array_count: 3

array_id 0: accel_x
unit: METER_PER_SEC2

array_id 1: accel_y
unit: METER_PER_SEC2

array_id 2: accel_z
unit: METER_PER_SEC2
```

### 14.17 GYROSCOPE 陀螺仪

```text
data_type: GYROSCOPE
array_count: 3

array_id 0: gyro_x
unit: DEGREE_PER_SEC

array_id 1: gyro_y
unit: DEGREE_PER_SEC

array_id 2: gyro_z
unit: DEGREE_PER_SEC
```

### 14.18 MAGNETOMETER 磁力计

```text
data_type: MAGNETOMETER
array_count: 3

array_id 0: mag_x
unit: MICRO_TESLA

array_id 1: mag_y
unit: MICRO_TESLA

array_id 2: mag_z
unit: MICRO_TESLA
```

### 14.19 ATTITUDE 姿态

姿态可以用欧拉角或四元数。初期建议二选一，不要在同一个传感器里混用。

欧拉角：

```text
data_type: ATTITUDE
array_count: 3

array_id 0: roll_deg
unit: DEGREE

array_id 1: pitch_deg
unit: DEGREE

array_id 2: yaw_deg
unit: DEGREE
```

四元数：

```text
data_type: ATTITUDE
array_count: 4

array_id 0: quat_w
unit: NONE

array_id 1: quat_x
unit: NONE

array_id 2: quat_y
unit: NONE

array_id 3: quat_z
unit: NONE
```

### 14.20 GNSS/GPS

```text
data_type: GNSS
array_count: 7

array_id 0: latitude_deg
unit: DEGREE

array_id 1: longitude_deg
unit: DEGREE

array_id 2: altitude_m
unit: METER

array_id 3: speed_mps
unit: METER_PER_SEC

array_id 4: heading_deg
unit: DEGREE

array_id 5: satellite_count
unit: COUNT

array_id 6: fix_quality
unit: COUNT
```

### 14.21 BAROMETER 气压计

```text
data_type: BAROMETER
array_count: 3

array_id 0: pressure_hpa
unit: HPA

array_id 1: altitude_m
unit: METER

array_id 2: temperature_c
unit: CELSIUS
```

### 14.22 RADAR_SUMMARY 雷达摘要

雷达原始点云不建议直接放入普通 `SENSOR_DATA`。这里定义摘要数据。

```text
data_type: RADAR_SUMMARY
array_count: 5

array_id 0: object_count
unit: COUNT

array_id 1: nearest_distance_m
unit: METER

array_id 2: farthest_distance_m
unit: METER

array_id 3: average_speed_mps
unit: METER_PER_SEC

array_id 4: frame_rate_hz
unit: HERTZ
```

### 14.23 LIDAR_SUMMARY 激光雷达摘要

```text
data_type: LIDAR_SUMMARY
array_count: 4

array_id 0: point_count
unit: COUNT

array_id 1: min_range_m
unit: METER

array_id 2: max_range_m
unit: METER

array_id 3: scan_rate_hz
unit: HERTZ
```

### 14.24 CAMERA_SUMMARY 相机摘要

```text
data_type: CAMERA_SUMMARY
array_count: 6

array_id 0: width
unit: COUNT

array_id 1: height
unit: COUNT

array_id 2: fps
unit: HERTZ

array_id 3: exposure_us
unit: NONE

array_id 4: gain
unit: NONE

array_id 5: frame_drop_count
unit: COUNT
```

### 14.25 GAS 气体浓度

```text
data_type: GAS
array_count: 1

array_id 0: concentration_ppm
unit: PPM
```

不同气体类型由 `NODE_INFO` 或 sensor 元数据说明，例如 CO、CO2、CH4、O2。

### 14.26 VIBRATION 振动

```text
data_type: VIBRATION
array_count: 4

array_id 0: accel_x
unit: METER_PER_SEC2

array_id 1: accel_y
unit: METER_PER_SEC2

array_id 2: accel_z
unit: METER_PER_SEC2

array_id 3: frequency_hz
unit: HERTZ
```

### 14.27 FORCE 力

```text
data_type: FORCE
array_count: 1

array_id 0: force_value
unit: NEWTON
```

第一版已定义 `NEWTON` 单位枚举，力数据应优先使用 `NEWTON`。

## 15. SENSOR_DATA 包示例

以下 JSON 仅用于说明 payload 字段含义，实际 `SENSOR_DATA` 为二进制 payload；`l2_id` 来自 `FrameHeader.l2_id`，数组名称来自 `NODE_INFO` 或接收端内置类型表。

### 15.1 电压数据

```json
{
  "l3_id": 11,
  "sensor_id": 3,
  "data_type": "VOLTAGE",
  "data_format": "FLOAT32",
  "sample_count": 1,
  "array_count": 1,
  "arrays": [
    {
      "array_id": 0,
      "unit": "VOLT",
      "values": [24.1]
    }
  ]
}
```

### 15.2 电流数据

```json
{
  "l3_id": 11,
  "sensor_id": 4,
  "data_type": "CURRENT",
  "data_format": "FLOAT32",
  "sample_count": 1,
  "array_count": 1,
  "arrays": [
    {
      "array_id": 0,
      "unit": "AMPERE",
      "values": [1.25]
    }
  ]
}
```

### 15.3 IMU 数据

```json
{
  "l3_id": 12,
  "sensor_id": 1,
  "data_type": "IMU",
  "data_format": "FLOAT32",
  "sample_count": 1,
  "array_count": 10,
  "arrays": [
    { "array_id": 0, "unit": "METER_PER_SEC2", "values": [0.01] },
    { "array_id": 1, "unit": "METER_PER_SEC2", "values": [0.02] },
    { "array_id": 2, "unit": "METER_PER_SEC2", "values": [9.80] },
    { "array_id": 3, "unit": "DEGREE_PER_SEC", "values": [0.00] },
    { "array_id": 4, "unit": "DEGREE_PER_SEC", "values": [0.01] },
    { "array_id": 5, "unit": "DEGREE_PER_SEC", "values": [0.00] },
    { "array_id": 6, "unit": "MICRO_TESLA", "values": [12.3] },
    { "array_id": 7, "unit": "MICRO_TESLA", "values": [-4.2] },
    { "array_id": 8, "unit": "MICRO_TESLA", "values": [36.8] },
    { "array_id": 9, "unit": "CELSIUS", "values": [38.5] }
  ]
}
```

### 15.4 多采样点 IMU 数据

`sample_count > 1` 时，每个数组的 `values` 长度也应等于 `sample_count`。

```json
{
  "data_type": "IMU",
  "sample_count": 3,
  "array_count": 6,
  "arrays": [
    { "array_id": 0, "values": [0.01, 0.02, 0.01] },
    { "array_id": 1, "values": [0.02, 0.01, 0.02] },
    { "array_id": 2, "values": [9.80, 9.81, 9.80] },
    { "array_id": 3, "values": [0.00, 0.01, 0.00] },
    { "array_id": 4, "values": [0.01, 0.01, 0.00] },
    { "array_id": 5, "values": [0.00, 0.00, 0.01] }
  ]
}
```

## 16. NODE_INFO 与 SENSOR_DATA 配对示例

以下示例展示同一个 `(l2_id, l3_id, sensor_id)` 下，`NODE_INFO` 和 `SENSOR_DATA` 如何一一对应。

`NODE_INFO`：

```json
{
  "l3_id": 12,
  "sensor_id": 1,
  "name": "IMU-01",
  "data_type": "IMU",
  "sample_rate_hz": 100,
  "array_descriptors": [
    { "array_id": 0, "name": "accel_x", "unit": "METER_PER_SEC2" },
    { "array_id": 1, "name": "accel_y", "unit": "METER_PER_SEC2" },
    { "array_id": 2, "name": "accel_z", "unit": "METER_PER_SEC2" },
    { "array_id": 3, "name": "gyro_x", "unit": "DEGREE_PER_SEC" },
    { "array_id": 4, "name": "gyro_y", "unit": "DEGREE_PER_SEC" },
    { "array_id": 5, "name": "gyro_z", "unit": "DEGREE_PER_SEC" }
  ]
}
```

对应的 `SENSOR_DATA`：

```json
{
  "l3_id": 12,
  "sensor_id": 1,
  "data_type": "IMU",
  "data_format": "FLOAT32",
  "sample_count": 1,
  "array_count": 6,
  "arrays": [
    { "array_id": 0, "values": [0.01] },
    { "array_id": 1, "values": [0.02] },
    { "array_id": 2, "values": [9.80] },
    { "array_id": 3, "values": [0.00] },
    { "array_id": 4, "values": [0.01] },
    { "array_id": 5, "values": [0.00] }
  ]
}
```

## 17. 解析规则

- `data_type` 决定默认数组含义。
- `array_id` 在同一个 `data_type` 内必须稳定。
- `NODE_INFO` 和 `SENSOR_DATA` 必须使用同一个 `(FrameHeader.l2_id, payload.l3_id, payload.sensor_id)` 作为 key。
- `array_count` 必须等于实际携带的 `DataArray` 数量。
- `value_count` 必须等于当前数组内的数值数量。
- 对普通周期数据，所有数组的 `value_count` 应等于 `sample_count`。
- 对缺失字段，不建议填 0，应直接不上传该数组，并通过 `array_count` 反映实际数组数量。
- 对无效采样，建议通过 `quality` 或告警包描述，不要混入正常数值。
- 接收端必须先校验 `FrameHeader` 和 `payload_crc32`，再解析 payload。
- 接收端必须拒绝超过 1200 字节的 datagram。

## 18. 初期建议实现范围

第一版建议优先支持以下类型：

- `VOLTAGE`
- `CURRENT`
- `TEMPERATURE`
- `MOTOR`
- `BATTERY`
- `IMU`
- `GNSS`
- `DISTANCE`
- `SWITCH`
- `RADAR_SUMMARY`

这些类型足够覆盖电源、电机、姿态、定位、测距和状态类传感器。等 UDP 接收、状态存储和 TUI 展示稳定后，再扩展其他类型。

## 19. 后续待定问题

- `l2_id`、`l3_id`、`sensor_id` 是烧录固定值，还是由配置文件分配。
- 高频 IMU 是否由第二层降采样后上传。
- 雷达、相机、点云是否只上传摘要，原始数据是否另走文件或专用通道。
- 后续多 L3 时，L2 采用 SLE 1 对多连接、轮询接收还是多连接事件驱动。
