// main.cpp
// -----------------------------------------------------------------------------
// 这个示例使用 FTXUI 构建一个全屏 TUI（终端用户界面）仪表盘。
// 布局结构如下：
//
// ┌──────────────────────────────────────────────────────────────┐
// │                 My FTXUI TUI Dashboard                      │
// │  ┌───────────────────────┬────────────────────────────────┐  │
// │  │      Left Top         │                                │  │
// │  │  (状态信息窗口)        │                                │  │
// │  ├───────────────────────┤        Right Main Window       │  │
// │  │    Left Bottom        │      (主内容显示区域)           │  │
// │  │ (日志/菜单/控制项)      │                                │  │
// │  └───────────────────────┴────────────────────────────────┘  │
// └──────────────────────────────────────────────────────────────┘
//
// 交互：按 q 或 Esc 退出程序。
// -----------------------------------------------------------------------------

// captured_mouse.hpp：鼠标捕获相关组件（本例未直接使用，但常随 component
// 一起引入）
#include <ftxui/component/captured_mouse.hpp>
// component.hpp：FTXUI 组件系统（Renderer / CatchEvent / Event 等）
#include <ftxui/component/component.hpp>
// screen_interactive.hpp：交互屏幕（全屏、循环事件处理）
#include <ftxui/component/screen_interactive.hpp>
// elements.hpp：DOM 元素（text / vbox / hbox / window / paragraph / separator
// 等）
#include <ftxui/dom/elements.hpp>

int
main ()
{
    // 使用 FTXUI 命名空间，避免每次都写 ftxui:: 前缀。
    using namespace ftxui;

    // 创建一个全屏交互终端。
    // Fullscreen() 会占用整个终端窗口，并接管键盘事件循环。
    auto screen = ScreenInteractive::Fullscreen ();

    // Renderer：用于“每一帧”构建界面树（Element）。
    // 这里用 lambda 返回当前界面的 DOM 结构。
    auto renderer = Renderer (
        [&]
            {
                // ------------------------- 左上窗口 -------------------------
                // window(标题, 内容, 边框样式)
                // hcenter：标题居中
                // bold：标题加粗
                // vbox：垂直堆叠内容
                // flex：允许该块在父容器中弹性伸缩
                auto left_top_window
                    = window (text ("Left Top") | hcenter | bold,
                              vbox ({
                                  text ("这里是左上窗口"),
                                  separator (),
                                  text ("可以放状态信息"),
                                  text ("例如：CPU / Memory / Device"),
                              }) | flex,
                              LIGHT);

                // ------------------------- 左下窗口 -------------------------
                auto left_bottom_window
                    = window (text ("Left Bottom") | hcenter | bold,
                              vbox ({
                                  text ("这里是左下窗口"),
                                  separator (),
                                  text ("可以放日志、菜单或控制项"),
                                  text ("Press q / Esc to quit"),
                              }) | flex,
                              ROUNDED);

                // ------------------------- 右侧主窗口 -----------------------
                // paragraph：自动换行的段落文本，适合显示说明/长文本。
                auto right_window = window (
                    text ("Right Main Window") | hcenter | bold,
                    vbox ({
                        text ("这里是右侧大窗口"),
                        separator (),
                        paragraph (
                            "This area can be used as the main view. "
                            "For example: charts, device data, serial output, "
                            "configuration table, or real-time monitoring "
                            "panel."),
                    }) | flex,
                    ROUNDED);

                // ------------------------- 左侧面板 -------------------------
                // 把左上、左下窗口上下排列。
                // size(WIDTH, EQUAL, 40)：固定左侧面板宽度为 40 列字符。
                auto left_panel = vbox ({
                                      left_top_window | flex,
                                      left_bottom_window | flex,
                                  })
                                  | size (WIDTH, EQUAL, 40);

                // ------------------------- 主布局 ---------------------------
                // hbox：左右并排。
                // 左侧固定 40 列，右侧弹性填充剩余空间。
                // 最后再加 flex，确保整体占满可用区域。
                auto main_layout = hbox ({
                                       left_panel,
                                       right_window | flex,
                                   })
                                   | flex;

                // ------------------------- 最外层窗口 -----------------------
                // 给整个布局再包一层总标题窗口。
                auto outer_window = window (text (" My FTXUI TUI Dashboard ")
                                                | hcenter | bold,
                                            main_layout, ROUNDED);

                // 返回最终渲染元素。
                return outer_window | flex;
            });

    // CatchEvent：对 renderer 包一层事件拦截。
    // 在这里处理快捷键（q / Esc 退出）。
    auto component = CatchEvent (renderer,
                                 [&] (Event event)
                                     {
                                         // 如果按下 q 或 Esc，则触发退出循环。
                                         if (event == Event::Character ('q')
                                             || event == Event::Escape)
                                             {
                                                 // ExitLoopClosure()
                                                 // 返回一个可调用对象，调用后退出
                                                 // screen.Loop。
                                                 screen.ExitLoopClosure () ();
                                                 return true; // 事件已处理
                                             }
                                         return false; // 未处理，交给其他组件
                                     });

    // 进入事件循环：持续渲染 + 处理输入，直到调用 ExitLoopClosure 退出。
    screen.Loop (component);

    return 0;
}
