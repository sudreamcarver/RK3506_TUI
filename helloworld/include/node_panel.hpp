#ifndef NODE_PANEL_HPP
#define NODE_PANEL_HPP

#include "ftxui/component/component_base.hpp"
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

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
    NodePanel ();

    ftxui::Component Component ();
    ftxui::Element RenderNodeListWindow ();
    ftxui::Element RenderDetailWindow ();

  private:
    std::vector<NodeInfo> nodes_;
    std::vector<std::string> node_names_;
    int selected_node_ = 0;

    ftxui::Component node_menu_;
};

#endif
