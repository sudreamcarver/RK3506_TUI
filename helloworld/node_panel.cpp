// node_panel.cpp
// -----------------------------------------------------------------------------
// NodePanel 的实现文件。
// 主要负责：
// 1) 初始化示例节点数据
// 2) 生成 FTXUI Menu 组件
// 3) 渲染节点列表窗口与详情窗口
// -----------------------------------------------------------------------------

#include "node_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

NodePanel::NodePanel ()
{
    // 初始化示例节点。
    // 实际项目中可替换为：设备发现结果、配置文件、网络状态回调等。
    nodes_ = {
        { "Motor", "电机节点\n\n状态: Online\n转速: 1200 RPM\n电流: 1.25 "
                   "A\n温度: 42 C" },
        { "Radar", "雷达节点\n\n状态: Online\nIP: 192.168.1.10\n点云数量: "
                   "2048\n刷新率: 10 Hz" },
        { "Camera", "相机节点\n\n状态: Offline\n分辨率: 1280x720\n帧率: 30 "
                    "FPS\n曝光: Auto" },
        { "IMU", "IMU 节点\n\n状态: Online\nAccel: 0.01, 0.02, 9.80\nGyro: "
                 "0.00, 0.01, 0.00" },
        { "GPS", "GPS 节点\n\n状态: Searching\nLatitude: --\nLongitude: "
                 "--\nSatellite: 0" },
    };

    // 从 nodes_ 提取名称，作为 Menu 的选项列表。
    for (const auto &node : nodes_)
        {
            node_names_.push_back (node.name);
        }

    // 创建菜单组件：
    // - 第一个参数：选项文本数组
    // - 第二个参数：当前选中索引（双向绑定）
    node_menu_ = ftxui::Menu (&node_names_, &selected_node_);
}

ftxui::Component
NodePanel::Component ()
{
    // 将内部菜单组件暴露给外部。
    // 外部可把它加入容器，获得焦点和键盘事件处理能力。
    return node_menu_;
}

ftxui::Element
NodePanel::RenderNodeListWindow ()
{
    using namespace ftxui;

    // 渲染左侧“节点列表”窗口。
    // node_menu_->Render() 会根据 selected_node_ 自动高亮当前项。
    return window (text ("Nodes") | hcenter | bold,
                   node_menu_->Render () | flex, ROUNDED);
}

ftxui::Element
NodePanel::RenderDetailWindow ()
{
    using namespace ftxui;

    // 依据当前选中索引，取对应节点。
    // 使用 at() 而非 []，可在越界时抛异常，便于调试。
    const auto &node = nodes_.at (static_cast<std::size_t> (selected_node_));

    // 渲染右侧“节点详情”窗口。
    // paragraph() 支持自动换行，适合展示多行说明。
    return window (text ("Node Detail: " + node.name) | hcenter | bold,
                   vbox ({
                       paragraph (node.content),
                   }) | flex,
                   ROUNDED);
}
