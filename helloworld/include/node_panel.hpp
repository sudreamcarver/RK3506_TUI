#ifndef NODE_PANEL_HPP
#define NODE_PANEL_HPP

// node_panel.hpp
// -----------------------------------------------------------------------------
// NodePanel 模块：
// - 管理“节点列表 + 节点详情”的数据与渲染逻辑
// - 对外提供一个可聚焦组件（Menu）用于键盘导航
// - 提供两个窗口渲染函数：
//   1) RenderNodeListWindow()   左侧节点列表窗口
//   2) RenderDetailWindow()     右侧节点详情窗口
// -----------------------------------------------------------------------------

// component_base.hpp：FTXUI 组件基类定义
#include "ftxui/component/component_base.hpp"
// component.hpp：Component / Menu 等组件接口
#include <ftxui/component/component.hpp>
// elements.hpp：Element / window / text / vbox / paragraph 等 DOM 元素
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>

// 单个节点的数据模型。
// name    : 列表中显示的节点名称
// content : 详情区域显示的多行文本
struct NodeInfo
{
    std::string name;
    std::string content;
};

// NodePanel：封装节点 UI 的状态与行为。
class NodePanel
{
  public:
    // 构造函数：初始化默认节点数据、菜单项与选中索引。
    NodePanel ();

    // 返回内部菜单组件，供外层 Container/CatchEvent 接入焦点系统。
    ftxui::Component Component ();

    // 渲染“节点列表”窗口（通常放左侧）。
    ftxui::Element RenderNodeListWindow ();

    // 渲染“节点详情”窗口（通常放右侧）。
    ftxui::Element RenderDetailWindow ();

  private:
    // 原始节点数据。
    std::vector<NodeInfo> nodes_;

    // Menu 组件需要 string 列表，这里缓存节点名数组。
    std::vector<std::string> node_names_;

    // 当前选中节点索引（与 Menu 绑定）。
    int selected_node_ = 0;

    // FTXUI 菜单组件：负责上下移动、高亮、回车等交互。
    ftxui::Component node_menu_;
};

#endif
