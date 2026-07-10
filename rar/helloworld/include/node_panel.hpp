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

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct NodeInfo
{
    std::string key;

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
        all_nodes_.push_back ({ name, name, content });

        // node_menu_ 绑定的是 node_names_ 和 selected_node_ 的地址。
        // 运行中更新节点时只改这两个对象本身，避免替换组件后 Renderer 仍持有旧组件。
        ApplyFilter ();

        // 防止选中索引越界。当前 AddNode() 只会追加数据，正常不会越界；
        // 这里保留保护逻辑，便于未来扩展删除/清空节点时复用。
        if (selected_node_ >= static_cast<int> (nodes_.size ()))
            {
                selected_node_ = static_cast<int> (
                    nodes_.empty () ? 0 : nodes_.size () - 1);
            }
    }

    void
    SetNodes (const std::vector<NodeInfo> &nodes)
    {
        all_nodes_ = nodes;
        ApplyFilter ();
    }

    void
    SetFilter (const std::string &filter)
    {
        if (filter_ == filter)
            {
                return;
            }

        filter_ = filter;
        ApplyFilter ();
    }

    void
    SetSearchActive (bool active)
    {
        search_active_ = active;
    }

    void
    ClearFilter ()
    {
        filter_.clear ();
        ApplyFilter ();
    }

    const std::string &
    Filter () const
    {
        return filter_;
    }

    bool
    SearchActive () const
    {
        return search_active_;
    }

    void
    AppendFilterText (const std::string &text)
    {
        filter_ += text;
        ApplyFilter ();
    }

    void
    PopFilterChar ()
    {
        if (!filter_.empty ())
            {
                filter_.pop_back ();
                ApplyFilter ();
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

    const std::vector<NodeInfo> &
    AllNodes () const
    {
        return all_nodes_;
    }

    std::string
    SelectedKey () const
    {
        if (nodes_.empty ())
            {
                return "";
            }

        return nodes_.at (static_cast<std::size_t> (selected_node_)).key;
    }

    void
    ClampSelectedNode ()
    {
        if (nodes_.empty ())
            {
                selected_node_ = 0;
            }
        else if (selected_node_ >= static_cast<int> (nodes_.size ()))
            {
                selected_node_ = static_cast<int> (nodes_.size () - 1U);
            }
        else if (selected_node_ < 0)
            {
                selected_node_ = 0;
            }
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

        const auto total = static_cast<int> (all_nodes_.size ());
        const auto filtered = static_cast<int> (nodes_.size ());
        const auto current = filtered == 0 ? 0 : selected_node_ + 1;
        auto title = "Nodes " + std::to_string (current) + "/"
                     + std::to_string (filtered);
        if (filtered != total)
            {
                title += " of " + std::to_string (total);
            }
        if (search_active_ || !filter_.empty ())
            {
                title += " /" + filter_;
                if (search_active_)
                    {
                        title += "_";
                    }
            }

        return window (
            text (title) | hcenter | bold,
            node_menu_->Render () | frame | flex, ROUNDED);
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

        auto status = ExtractStatus (node.content);
        auto metrics = ExtractMetrics (node.content);

        return window (text ("Node Detail: " + node.name) | hcenter | bold,
                       vbox ({
                           RenderSummary (node, status),
                           separator (),
                           RenderMetrics (metrics) | flex,
                           separator (),
                           text ("Raw Detail") | bold,
                           paragraph (node.content) | dim,
                       }) | flex,
                       ROUNDED);
    }

  private:
    static std::string
    ToLower (std::string value)
    {
        std::transform (value.begin (),
                        value.end (),
                        value.begin (),
                        [] (unsigned char character)
                            {
                                return static_cast<char> (
                                    std::tolower (character));
                            });
        return value;
    }

    bool
    MatchesFilter (const NodeInfo &node) const
    {
        if (filter_.empty ())
            {
                return true;
            }

        const auto filter = ToLower (filter_);
        return ToLower (node.name).find (filter) != std::string::npos
               || ToLower (node.content).find (filter) != std::string::npos;
    }

    void
    ApplyFilter ()
    {
        nodes_.clear ();
        node_names_.clear ();

        for (const auto &node : all_nodes_)
            {
                if (MatchesFilter (node))
                    {
                        nodes_.push_back (node);
                        node_names_.push_back (node.name);
                    }
            }

        ClampSelectedNode ();
    }

    static std::string
    Trim (const std::string &value)
    {
        const auto first = value.find_first_not_of (" \t\r\n");
        if (first == std::string::npos)
            {
                return "";
            }

        const auto last = value.find_last_not_of (" \t\r\n");
        return value.substr (first, last - first + 1U);
    }

    static std::vector<std::string>
    SplitLines (const std::string &content)
    {
        std::vector<std::string> lines;
        std::istringstream stream (content);
        std::string line;

        while (std::getline (stream, line))
            {
                lines.push_back (Trim (line));
            }

        return lines;
    }

    static std::string
    ExtractStatus (const std::string &content)
    {
        for (const auto &line : SplitLines (content))
            {
                const auto separator_position = line.find (':');
                if (separator_position == std::string::npos)
                    {
                        continue;
                    }

                const auto key = Trim (line.substr (0U, separator_position));
                if (key == "状态" || key == "Status")
                    {
                        return Trim (line.substr (separator_position + 1U));
                    }
            }

        return "Unknown";
    }

    static std::vector<std::pair<std::string, std::string>>
    ExtractMetrics (const std::string &content)
    {
        std::vector<std::pair<std::string, std::string>> metrics;

        for (const auto &line : SplitLines (content))
            {
                const auto separator_position = line.find (':');
                if (separator_position == std::string::npos)
                    {
                        continue;
                    }

                auto key = Trim (line.substr (0U, separator_position));
                auto value = Trim (line.substr (separator_position + 1U));
                if (key.empty () || value.empty () || key == "状态"
                    || key == "Status")
                    {
                        continue;
                    }

                metrics.push_back ({ key, value });
            }

        return metrics;
    }

    static ftxui::Element
    RenderSummary (const NodeInfo &node, const std::string &status)
    {
        using namespace ftxui;

        auto status_value = text (status) | bold;
        if (status == "Online")
            {
                status_value = status_value | color (Color::Green);
            }
        else if (status == "Offline")
            {
                status_value = status_value | color (Color::Red);
            }
        else if (status == "Searching")
            {
                status_value = status_value | color (Color::Yellow);
            }
        else
            {
                status_value = status_value | color (Color::GrayDark);
            }

        return hbox ({
                   vbox ({
                       text ("Selected Node") | dim,
                       text (node.name) | bold,
                   }) | flex,
                   separator (),
                   vbox ({
                       text ("Status") | dim,
                       status_value,
                   }) | flex,
                   separator (),
                   vbox ({
                       text ("Updated") | dim,
                       text ("Static sample") | bold,
                   }) | flex,
               })
               | border;
    }

    static ftxui::Element
    RenderMetricCard (const std::string &label, const std::string &value)
    {
        using namespace ftxui;

        return vbox ({
                   text (label) | dim,
                   text (value) | bold,
               })
               | border | size (HEIGHT, EQUAL, 4) | flex;
    }

    static ftxui::Element
    RenderMetrics (
        const std::vector<std::pair<std::string, std::string>> &metrics)
    {
        using namespace ftxui;

        if (metrics.empty ())
            {
                return vbox ({
                           text ("Metrics") | bold,
                           paragraph ("No metric fields available.") | dim,
                       })
                       | flex;
            }

        Elements rows;
        rows.push_back (text ("Metrics") | bold);

        for (std::size_t index = 0U; index < metrics.size (); index += 2U)
            {
                Elements columns;
                columns.push_back (RenderMetricCard (metrics[index].first,
                                                     metrics[index].second));

                if (index + 1U < metrics.size ())
                    {
                        columns.push_back (
                            RenderMetricCard (metrics[index + 1U].first,
                                              metrics[index + 1U].second));
                    }
                else
                    {
                        columns.push_back (filler ());
                    }

                rows.push_back (hbox (columns));
            }

        return vbox (rows) | flex;
    }

    // 过滤后的节点集合。每个元素包含节点名和详情文本。
    std::vector<NodeInfo> nodes_;

    // 完整节点集合，用于搜索过滤。
    std::vector<NodeInfo> all_nodes_;

    // 左侧菜单显示文本缓存。FTXUI Menu 接收的是 string 列表，因此它和
    // nodes_ 分开保存；新增节点时必须与 nodes_ 同步追加。
    std::vector<std::string> node_names_;

    // 当前选中的节点索引。该变量由 FTXUI Menu 根据键盘事件更新，右侧详情
    // 渲染时使用它读取对应的 NodeInfo。
    int selected_node_ = 0;

    std::string filter_;
    bool search_active_ = false;

    // FTXUI 菜单组件。它持有 node_names_ 和 selected_node_ 的地址，因此这两个
    // 成员必须比 node_menu_ 活得更久；当前成员声明顺序满足这个要求。
    ftxui::Component node_menu_ = ftxui::Menu (&node_names_, &selected_node_);
};

#endif
