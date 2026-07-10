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

#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::size_t kMaxLogLines = 6U;

std::string
ParseOption (int argc,
             char **argv,
             const std::string &option,
             const std::string &default_value)
{
    for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == option && i + 1 < argc)
                {
                    return argv[i + 1];
                }
        }

    return default_value;
}

std::string
DecodeStateField (const std::string &value)
{
    std::string decoded;
    decoded.reserve (value.size ());

    for (std::size_t i = 0U; i < value.size (); ++i)
        {
            if (value[i] != '\\' || i + 1U >= value.size ())
                {
                    decoded.push_back (value[i]);
                    continue;
                }

            ++i;
            if (value[i] == 'n')
                {
                    decoded.push_back ('\n');
                }
            else if (value[i] == 't')
                {
                    decoded.push_back ('\t');
                }
            else
                {
                    decoded.push_back (value[i]);
                }
        }

    return decoded;
}

std::vector<NodeInfo>
ReadNodeStateFile (const std::string &state_path)
{
    std::ifstream state (state_path);
    if (!state.is_open ())
        {
            return {};
        }

    std::vector<NodeInfo> nodes;
    std::string line;
    while (std::getline (state, line))
        {
            const auto separator = line.find ('\t');
            if (separator == std::string::npos)
                {
                    continue;
                }

            const auto second_separator = line.find ('\t', separator + 1U);
            if (second_separator == std::string::npos)
                {
                    auto name = DecodeStateField (line.substr (0U, separator));
                    auto content = DecodeStateField (line.substr (separator + 1U));
                    if (!name.empty ())
                        {
                            nodes.push_back ({ name, name, content });
                        }
                    continue;
                }

            auto key = DecodeStateField (line.substr (0U, separator));
            auto name = DecodeStateField (
                line.substr (separator + 1U, second_separator - separator - 1U));
            auto content = DecodeStateField (line.substr (second_separator + 1U));
            if (!name.empty ())
                {
                    nodes.push_back ({ key, name, content });
                }
        }

    return nodes;
}

void
AppendDeleteRequest (const std::string &delete_path, const std::string &key)
{
    if (key.empty ())
        {
            return;
        }

    std::ofstream deletes (delete_path, std::ios::app);
    if (deletes.is_open ())
        {
            deletes << key << '\n';
        }
}

void
AppendUiLog (const std::string &log_path, const std::string &message)
{
    std::ofstream log (log_path, std::ios::app);
    if (log.is_open ())
        {
            log << message << '\n';
        }
}

std::vector<std::string>
ReadLastLogLines (const std::string &log_path)
{
    std::ifstream log (log_path);
    if (!log.is_open ())
        {
            return { "waiting for UDP receive log: " + log_path };
        }

    std::deque<std::string> lines;
    std::string line;
    while (std::getline (log, line))
        {
            if (line.empty ())
                {
                    continue;
                }
            lines.push_back (line);
            if (lines.size () > kMaxLogLines)
                {
                    lines.pop_front ();
                }
        }

    if (lines.empty ())
        {
            return { "no received UDP data yet" };
        }

    return { lines.begin (), lines.end () };
}

ftxui::Element
RenderReceiveLogWindow (const std::string &log_path)
{
    using namespace ftxui;

    Elements rows;
    for (const auto &line : ReadLastLogLines (log_path))
        {
            rows.push_back (paragraph (line));
        }

    return window (text ("Receive Log") | hcenter | bold,
                   vbox (rows) | flex);
}

} // namespace

int
main (int argc, char **argv)
{
    using namespace ftxui;

    const auto log_path = ParseOption (
        argc, argv, "--log-file", "/tmp/rk3506_udp_received.log");
    const auto state_path = ParseOption (
        argc, argv, "--state-file", "/tmp/rk3506_udp_nodes.tsv");
    const auto delete_path = ParseOption (
        argc, argv, "--delete-file", "/tmp/rk3506_udp_delete.tsv");
    auto screen = ScreenInteractive::Fullscreen ();
    std::atomic<bool> keep_refreshing = true;
    bool search_active = false;

    // 创建节点面板对象（封装了数据、菜单组件和两个渲染窗口）。
    NodePanel node_panel;

    // 渲染器：每帧根据当前选中节点重绘列表与详情。
    auto renderer = Renderer (
        node_panel.Component (),
        [&]
            {
                const auto live_nodes = ReadNodeStateFile (state_path);
                node_panel.SetNodes (live_nodes);
                node_panel.SetSearchActive (search_active);

                auto left_window = node_panel.RenderNodeListWindow ()
                                   | size (WIDTH, EQUAL, 36);

                auto receive_log_window = RenderReceiveLogWindow (log_path)
                                          | size (HEIGHT, EQUAL, 8);

                auto right_window = node_panel.RenderDetailWindow () | flex;

                auto node_layout = hbox ({ left_window, right_window }) | flex;

                auto main_layout
                    = vbox ({ receive_log_window, node_layout }) | flex;

                return window (text (" RK3506 Node Dashboard ") | hcenter
                                   | bold,
                               main_layout, ROUNDED)
                       | flex;
            });

    // 全局快捷键：q / Esc 退出。
    auto app = CatchEvent (renderer,
                           [&] (Event event)
                               {
                                   if (search_active)
                                       {
                                           if (event == Event::Escape)
                                               {
                                                   search_active = false;
                                                   node_panel.SetSearchActive (
                                                       false);
                                                   node_panel.ClearFilter ();
                                                   return true;
                                               }
                                           if (event == Event::Return)
                                               {
                                                   search_active = false;
                                                   node_panel.SetSearchActive (
                                                       false);
                                                   return true;
                                               }
                                           if (event == Event::Backspace)
                                               {
                                                   node_panel.PopFilterChar ();
                                                   return true;
                                               }
                                           if (event.is_character ())
                                               {
                                                   node_panel.AppendFilterText (
                                                       event.character ());
                                                   return true;
                                               }

                                           return true;
                                       }

                                   if (event == Event::Character ('/'))
                                       {
                                           search_active = true;
                                           node_panel.SetSearchActive (true);
                                           return true;
                                       }

                                   if (event == Event::Character ('d'))
                                       {
                                           const auto key
                                               = node_panel.SelectedKey ();
                                           AppendDeleteRequest (
                                               delete_path, key);
                                           if (!key.empty ())
                                               {
                                                   AppendUiLog (
                                                       log_path,
                                                       "TUI delete request: "
                                                           + key);
                                               }
                                           return true;
                                       }

                                   if (event == Event::Character ('q')
                                       || event == Event::Escape)
                                       {
                                           screen.ExitLoopClosure () ();
                                           keep_refreshing = false;
                                           return true;
                                       }
                                   return false;
                               });

    std::thread refresh_thread (
        [&]
            {
                while (keep_refreshing)
                    {
                        std::this_thread::sleep_for (
                            std::chrono::milliseconds (500));
                        screen.PostEvent (Event::Custom);
                    }
            });

    screen.Loop (app);
    keep_refreshing = false;
    if (refresh_thread.joinable ())
        {
            refresh_thread.join ();
        }
    return 0;
}
