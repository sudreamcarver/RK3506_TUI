# helloworld 项目现状说明（FTXUI）

> 路径：`/home/carver/RK3506_TUI/helloworld`
> 
> 本文档总结当前代码结构、模块职责、构建方式、交互行为与后续建议，便于继续开发与交接。

---

## 1. 项目目标与当前状态

该项目是一个基于 **FTXUI** 的终端界面（TUI）示例/原型，当前已从“静态三窗格示例”演进为“节点列表 + 节点详情”的可交互界面。

当前可实现：

- 全屏 TUI 显示
- 左侧节点列表菜单（可上下切换）
- 右侧显示当前节点详情
- `q` / `Esc` 快捷退出

---

## 2. 目录结构（核心文件）

```text
helloworld/
├── CMakeLists.txt
├── .gitignore
├── main.cpp
├── node_panel.cpp
├── include/
│   └── node_panel.hpp
├── cmake/
│   └── toolchain.cmake
├── build/           # 构建输出（已忽略）
└── build_x86/       # 构建输出（已忽略）
```

说明：

- `build/` 与 `build_x86/` 为构建目录，已加入 `.gitignore`。
- `main.cpp.bak` 等备份文件存在于工作目录中，但当前 Git 追踪重点是主代码与配置。

---

## 3. 模块设计

### 3.1 `NodeInfo` 数据结构

定义于 `include/node_panel.hpp`：

- `name`：节点名称（列表中展示）
- `content`：节点详细信息（右侧详情展示）

### 3.2 `NodePanel` 类职责

`NodePanel` 封装了“节点 UI 子系统”的核心逻辑：

1. 管理节点数据集合（`nodes_`）
2. 维护菜单项文本（`node_names_`）
3. 维护当前选中索引（`selected_node_`）
4. 提供菜单组件（`Component()`）供主界面挂载
5. 提供两个渲染函数：
   - `RenderNodeListWindow()`：渲染左侧节点列表窗口
   - `RenderDetailWindow()`：渲染右侧节点详情窗口

### 3.3 主程序 `main.cpp` 职责

`main.cpp` 负责：

- 初始化 `ScreenInteractive::Fullscreen()`
- 创建 `NodePanel` 实例
- 使用 `Renderer(node_panel.Component(), ...)` 建立渲染与交互链路
- 拼装左右布局并显示外层窗口标题
- 使用 `CatchEvent` 处理全局退出键（`q` / `Esc`）

---

## 4. 当前交互行为

启动后：

- 左侧显示节点菜单（如 `Motor` / `Radar` / `Camera` / `IMU` / `GPS`）
- 使用键盘上下键切换选中项
- 右侧详情区域实时显示对应节点文本
- 按 `q` 或 `Esc` 退出

---

## 5. 构建系统说明（CMake）

### 5.1 关键配置

`CMakeLists.txt` 当前要点：

- C++ 标准：`C++17`
- 通过 `FetchContent` 引入 FTXUI
- FTXUI 源码目录：`/home/carver/Documents/FTXUI`
- 源文件收集：`*.cpp`
- 头文件目录：`include/`
- 编译选项较严格：`-Wall -Wextra -Wconversion -Wsign-conversion -Werror ...`

### 5.2 链接库

链接：

- `ftxui::screen`
- `ftxui::dom`
- `ftxui::component`

### 5.3 已验证编译

已成功执行：

```bash
cmake -S . -B build_x86
cmake --build build_x86 -j4
```

构建结果：`hellowWorld` 成功生成。

---

## 6. 已完成的近期改动总结

1. 为 `node_panel.hpp` 和 `node_panel.cpp` 增加详细中文注释
2. 将 `NodePanel` 成功接入 `main.cpp`
3. 新增 `.gitignore`，忽略构建目录：
   - `build/`
   - `build_x86/`
4. 编译验证通过并已提交

---

## 7. 后续开发建议

### 7.1 数据来源改造

目前节点数据是硬编码示例，建议改为：

- 配置文件加载（YAML/JSON）
- 网络/串口实时输入
- 设备扫描结果动态填充

### 7.2 详情区域增强

可扩展为：

- 分段字段展示（状态、指标、报警）
- 颜色高亮（Online/Offline/Warning）
- 实时刷新与时间戳

### 7.3 交互增强

可增加：

- `Enter` 打开节点操作菜单
- `r` 刷新数据
- `/` 搜索节点
- `Tab` 在不同面板间切换焦点

### 7.4 工程化建议

- 增加 `README.md`（快速构建、运行说明）
- 增加最小单元测试（数据层）
- 将 UI 逻辑与数据获取逻辑解耦（便于测试）

---

## 8. 快速运行参考

在项目目录执行：

```bash
cmake -S . -B build_x86
cmake --build build_x86 -j4
./build_x86/hellowWorld
```

退出：`q` 或 `Esc`。

---

## 9. 备注

- 项目名/可执行名当前为 `hellowWorld`（拼写保持与现有 CMake 一致）。
- 若后续要统一命名为 `helloWorld` 或其他，需要同步修改 CMake 与脚本引用。
