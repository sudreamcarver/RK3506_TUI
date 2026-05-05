#ifndef NODE_PANEL_HPP
#define NODE_PANEL_HPP

// node_panel.hpp (header-only)
// -----------------------------------------------------------------------------
// NodePanel 模块：
// - 管理“节点列表 + 节点详情”的数据与渲染逻辑
// - 对外提供可聚焦组件（Menu）用于键盘导航
// - 提供可扩展的节点注入接口，方便外部传感器/驱动层动态添加节点
// -----------------------------------------------------------------------------

#include "ftxui/component/component_base.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct NodeInfo
{
    std::string name;
    std::string content;
};

class NodePanel
{
  public:
    NodePanel ()
    {
        AddNode ("Motor",
                 "电机节点\n\n状态: Online\n转速: 1200 RPM\n电流: 1.25 "
                 "A\n温度: 42 C");
        AddNode ("Radar",
                 "雷达节点\n\n状态: Online\nIP: 192.168.1.10\n点云数量: "
                 "2048\n刷新率: 10 Hz");
        AddNode ("Camera",
                 "相机节点\n\n状态: Offline\n分辨率: 1280x720\n帧率: 30 "
                 "FPS\n曝光: Auto");
        AddNode ("IMU",
                 "IMU 节点\n\n状态: Online\nAccel: 0.01, 0.02, 9.80\nGyro: "
                 "0.00, 0.01, 0.00");
        AddNode ("GPS",
                 "GPS 节点\n\n状态: Searching\nLatitude: --\nLongitude: "
                 "--\nSatellite: 0");
    }

    // 外部传感器/驱动层可调用这个接口，动态往列表中追加节点。
    void
    AddNode (const std::string &name, const std::string &content)
    {
        nodes_.push_back ({ name, content });
        node_names_.push_back (name);

        // 重新绑定菜单，保证新增节点后菜单项同步更新。
        node_menu_ = ftxui::Menu (&node_names_, &selected_node_);

        // 防止选中索引越界。
        if (selected_node_ >= static_cast<int> (nodes_.size ()))
            {
                selected_node_ = static_cast<int> (
                    nodes_.empty () ? 0 : nodes_.size () - 1);
            }
    }

    // 直接暴露只读访问，便于外部读取当前节点集合。
    const std::vector<NodeInfo> &
    Nodes () const
    {
        return nodes_;
    }

    ftxui::Component
    Component ()
    {
        return node_menu_;
    }

    ftxui::Element
    RenderNodeListWindow ()
    {
        using namespace ftxui;

        return window (text ("Nodes") | hcenter | bold,
                       node_menu_->Render () | flex, ROUNDED);
    }

    ftxui::Element
    RenderDetailWindow ()
    {
        using namespace ftxui;

        if (nodes_.empty ())
            {
                return window (text ("Node Detail") | hcenter | bold,
                               paragraph ("No node available.") | flex,
                               ROUNDED);
            }

        const auto &node
            = nodes_.at (static_cast<std::size_t> (selected_node_));

        return window (text ("Node Detail: " + node.name) | hcenter | bold,
                       vbox ({
                           paragraph (node.content),
                       }) | flex,
                       ROUNDED);
    }

  private:
    std::vector<NodeInfo> nodes_;
    std::vector<std::string> node_names_;
    int selected_node_ = 0;
    ftxui::Component node_menu_ = ftxui::Menu (&node_names_, &selected_node_);
};

#endif
