#include "node_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

NodePanel::NodePanel ()
{
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

    for (const auto &node : nodes_)
        {
            node_names_.push_back (node.name);
        }

    node_menu_ = ftxui::Menu (&node_names_, &selected_node_);
}

ftxui::Component
NodePanel::Component ()
{
    return node_menu_;
}

ftxui::Element
NodePanel::RenderNodeListWindow ()
{
    using namespace ftxui;

    return window (text ("Nodes") | hcenter | bold,
                   node_menu_->Render () | flex, ROUNDED);
}

ftxui::Element
NodePanel::RenderDetailWindow ()
{
    using namespace ftxui;

    const auto &node = nodes_.at (static_cast<std::size_t> (selected_node_));

    return window (text ("Node Detail: " + node.name) | hcenter | bold,
                   vbox ({
                       paragraph (node.content),
                   }) | flex,
                   ROUNDED);
}
