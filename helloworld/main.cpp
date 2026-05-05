// main.cpp
// -----------------------------------------------------------------------------
// 该版本将 NodePanel 模块接入主界面：
// - 左侧显示节点列表（可用方向键切换）
// - 右侧显示当前选中节点详情
// - 按 q 或 Esc 退出
// -----------------------------------------------------------------------------

#include "node_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

int
main ()
{
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen ();

    // 创建节点面板对象（封装了数据、菜单组件和两个渲染窗口）。
    NodePanel node_panel;

    // 渲染器：每帧根据当前选中节点重绘列表与详情。
    auto renderer = Renderer (
        node_panel.Component (),
        [&]
            {
                auto left_window = node_panel.RenderNodeListWindow ()
                                   | size (WIDTH, EQUAL, 36);

                auto right_window = node_panel.RenderDetailWindow () | flex;

                auto main_layout = hbox ({ left_window, right_window }) | flex;

                return window (text (" RK3506 Node Dashboard ") | hcenter | bold,
                               main_layout, ROUNDED)
                       | flex;
            });

    // 全局快捷键：q / Esc 退出。
    auto app = CatchEvent (renderer,
                           [&] (Event event)
                               {
                                   if (event == Event::Character ('q')
                                       || event == Event::Escape)
                                       {
                                           screen.ExitLoopClosure () ();
                                           return true;
                                       }
                                   return false;
                               });

    screen.Loop (app);
    return 0;
}
