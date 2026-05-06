#ifndef NODE_PANEL_HPP
#define NODE_PANEL_HPP

// node_panel.hpp (header-only)
// -----------------------------------------------------------------------------
// NodePanel 模块：
// - 管理“节点列表 + 节点详情”的数据与渲染逻辑。
// - 对外提供可聚焦组件（Menu）用于键盘导航。
// - 提供可扩展的节点注入接口，方便外部传感器/驱动层动态添加节点。
//
// 设计说明：
// - 本文件采用 header-only 形式，当前项目规模较小，可以减少 .cpp/.hpp
//   拆分带来的同步成本。
// - nodes_ 是节点真实数据源，node_names_ 是 FTXUI Menu 使用的显示文本缓存。
//   外部不要直接修改 nodes_，否则 node_names_ 和菜单组件不会自动同步。
// - 外部新增节点统一通过 AddNode() 完成，函数内部会同时维护 nodes_、
//   node_names_ 和 node_menu_，保证 UI 列表与详情数据一致。
// -----------------------------------------------------------------------------

#include "ftxui/component/component_base.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstddef>
#include <string>
#include <vector>

struct NodeInfo
{
    // 节点显示名：用于左侧节点列表，也用于右侧详情窗口标题。
    std::string name;

    // 节点详情内容：用于右侧详情窗口，支持包含换行的多行文本。
    std::string content;
};

class NodePanel
{
  public:
    // 构造函数填充一组演示节点，方便没有外部传感器输入时也能看到界面效果。
    // 后续接入真实传感器/驱动层时，可以保留这些默认节点作为示例，也可以
    // 改成空列表并完全依赖外部调用 AddNode() 注入数据。
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
    //
    // 参数：
    // - name：节点名称，会显示在左侧菜单中。
    // - content：节点详情，会显示在右侧详情区域中。
    //
    // 注意：
    // - 追加节点时必须同时更新 nodes_ 与 node_names_。
    // - FTXUI Menu 绑定的是 node_names_ 和 selected_node_ 的地址，因此
    //   node_names_ 改变后重新创建 node_menu_，可以保证菜单项和最新数据一致。
    void
    AddNode (const std::string &name, const std::string &content)
    {
        nodes_.push_back ({ name, content });
        node_names_.push_back (name);

        // 重新绑定菜单，保证新增节点后菜单项同步更新。
        node_menu_ = ftxui::Menu (&node_names_, &selected_node_);

        // 防止选中索引越界。当前 AddNode() 只会追加数据，正常不会越界；
        // 这里保留保护逻辑，便于未来扩展删除/清空节点时复用。
        if (selected_node_ >= static_cast<int> (nodes_.size ()))
            {
                selected_node_ = static_cast<int> (
                    nodes_.empty () ? 0 : nodes_.size () - 1);
            }
    }

    // 直接暴露只读访问，便于外部读取当前节点集合。
    //
    // 返回 const 引用，允许外部读取 nodes_，但不允许直接写入。这样可以避免
    // 外部绕过 AddNode() 修改数据后，node_names_ 或 node_menu_ 没有同步更新。
    const std::vector<NodeInfo> &
    Nodes () const
    {
        return nodes_;
    }

    // 返回左侧菜单组件。主界面需要把这个组件传给 Renderer()，FTXUI 才能把
    // 键盘上下键等事件分发给菜单，实现选中项切换。
    ftxui::Component
    Component ()
    {
        return node_menu_;
    }

    // 渲染左侧节点列表窗口。
    //
    // node_menu_->Render() 会根据 node_names_ 和 selected_node_ 生成当前菜单
    // 视图；selected_node_ 会随着用户键盘操作自动变化。
    ftxui::Element
    RenderNodeListWindow ()
    {
        using namespace ftxui;

        return window (text ("Nodes") | hcenter | bold,
                       node_menu_->Render () | flex, ROUNDED);
    }

    // 渲染右侧节点详情窗口。
    //
    // 详情内容来自 nodes_[selected_node_]。由于 selected_node_ 是菜单组件共享
    // 的状态，用户切换左侧菜单时，右侧详情会在下一帧自动显示对应节点。
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

        // selected_node_ 是 int，而 vector 下标是 size_t；这里显式转换，避免
        // 严格编译选项下出现有符号/无符号转换警告。
        const auto &node = nodes_.at (
            static_cast<std::size_t> (selected_node_)); // 返回对应下标的内容

        return window (text ("Node Detail: " + node.name) | hcenter | bold,
                       vbox ({
                           paragraph (node.content),
                       }) | flex,
                       ROUNDED);
    }

  private:
    // 节点真实数据源。每个元素包含节点名和详情文本。
    std::vector<NodeInfo> nodes_;

    // 左侧菜单显示文本缓存。FTXUI Menu 接收的是 string 列表，因此它和
    // nodes_ 分开保存；新增节点时必须与 nodes_ 同步追加。
    std::vector<std::string> node_names_;

    // 当前选中的节点索引。该变量由 FTXUI Menu 根据键盘事件更新，右侧详情
    // 渲染时使用它读取对应的 NodeInfo。
    int selected_node_ = 0;

    // FTXUI 菜单组件。它持有 node_names_ 和 selected_node_ 的地址，因此这两个
    // 成员必须比 node_menu_ 活得更久；当前成员声明顺序满足这个要求。
    ftxui::Component node_menu_ = ftxui::Menu (&node_names_, &selected_node_);
};

#endif
