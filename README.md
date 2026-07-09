# VorteX

VorteX is a high-performance, interactive Terminal User Interface (TUI) file manager and directory router built in modern C++. Powered by the functional FTXUI library and standard filesystem vectors, it provides a lightning-fast, keyboard-driven navigation dashboard with zero dependency bloating. 

## Key Architecture & Features

- **Asynchronous Execution Nodes:** Offconnects heavy tasks by spawning detached threads for file operations, preventing rendering lag or frozen frames during external triggers.
- **Dynamic File System Indexing:** Automatically evaluates, tracks, and dynamically sorts directory paths, giving structural layout weight to directory nodes over files.
- **Autonomous Query Pipelines:** Features an adaptive Find Mode leveraging low-level case-insensitive substring searching algorithms to filter multi-column paths live.
- **Dual-Pane Interface Core:** Splits structural rendering layouts with highly visible boundary markers that clearly signal state shifts between file trees and text buffers.

---

## Keyboard Control Vector

Operating VORTEX is strictly decentralized from cursor control devices, relying entirely on direct keystroke event loops mapped for raw velocity:

- **Tab Key:** Instantly swaps systemic control context between the navigation list and the action layout box.
- **Arrow Keys (Up/Down):** Moves selection indices smoothly up and down across the discovered entries vector.
- **Return / Enter:** Validates entry markers; dives deep into subdirectory systems, jumps upwards, or deploys asynchronous background handlers for explicit raw files.
- **F / f Key:** Drops the operational layer directly into Find Mode, redirecting active alphanumeric characters to input search structures.
- **N / n Key:** Composes an immediate empty asset inside the local directory path using strings currently waiting in the input staging vector.
- **Delete Key:** Executes a non-blocking recursive elimination pass on the active item file vector block.
- **Escape Key:** Completely purges internal search variables and forcibly resets focus parameters back to standard list layouts.
- **Q / q Key:** Cleanly interrupts the full-screen terminal interaction cycle and returns access gracefully to the execution shell environment.

---

## Technical Maintenance Note
VorteX strictly obeys the foundational principles of minimized structural footprints. It encapsulates core state managers within isolated component render scopes, ensuring consistent tick performance even under high-density system structures. Part of the automated development toolsets pipeline.

---

## Fun Facts!
- This project took 4 months from me! There were 3 main attempts for this project.
1. Java + JavaFX + Fortran + C++ + C++ Bridge
 After this attempt, I stopped the project VorteX for about 2 months. I do not think that i should tell the reason it was terminated, but whatever. The reaseon is that all these modules could not communicate efficiently and JavaFX was not good for VorteX.
2. C++ CLI
 This one did not look pretty but more importantly, it did not work well. It had a lot of bugs. After this attempt, I stopped project VorteX again for approximately 1 month.
3. C++ FTXUI (TUI)
 This was the true final of the project VorteX. It looked so pretty and worked well without bugs.

- VorteX is inspired by [sxyazi/yazi](https://github.com/sxyazi/yazi) and developed to be an alternative for it.

- VorteX has a one-line edition source code in my other repository [OneLineHub](https://github.com/hypernova-developer/OneLineHub). 470 lines, compressed to ***one***!

---

## Appendix: Master Source Artifact

```cpp
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <system_error>

using namespace ftxui;
namespace fs = std::filesystem;

static std::string ToLower(std::string s)
{
    for (char& c : s)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

static bool CaseInsensitiveContains(std::string const& haystack, std::string const& needle)
{
    if (needle.empty())
    {
        return true;
    }
    std::string h = ToLower(haystack);
    std::string n = ToLower(needle);
    return h.find(n) != std::string::npos;
}

static std::string FormatBytes(std::uintmax_t bytes)
{
    static constexpr std::uintmax_t k = 1024;
    if (bytes < k)
    {
        return std::to_string(bytes) + " B";
    }
    std::uintmax_t v = bytes;
    int exp = 0;
    static constexpr const char* units[] = {"KiB", "MiB", "GiB", "TiB", "PiB"};
    while (v >= k && exp < 5)
    {
        v /= k;
        ++exp;
    }
    double out = static_cast<double>(bytes);
    for (int i = 0; i < exp; ++i)
    {
        out /= 1024.0;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1f %s", out, units[exp]);
    return std::string(buf);
}

static bool IsDirectoryFast(fs::directory_entry const& e)
{
    std::error_code ec;
    return e.is_directory(ec);
}

static std::uintmax_t ComputeDirectorySizeRecursive(fs::path const& root, std::atomic_bool& cancel)
{
    std::uintmax_t total = 0;
    std::error_code ec;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    for (; it != fs::recursive_directory_iterator(); ++it)
    {
        if (cancel.load(std::memory_order_relaxed))
        {
            return total;
        }
        std::error_code e2;
        auto const& entry = *it;
        if (entry.is_regular_file(e2))
        {
            std::uintmax_t sz = entry.file_size(e2);
            if (!e2)
            {
                total += sz;
            }
        }
    }
    return total;
}

struct ThreadPool
{
    explicit ThreadPool(std::size_t n) : stop(false)
    {
        for (std::size_t i = 0; i < n; ++i)
        {
            workers.emplace_back([this]()
            {
                for (;;)
                {
                    std::function<void()> job;
                    {
                        std::unique_lock<std::mutex> lk(m);
                        cv.wait(lk, [this]() { return stop.load(std::memory_order_relaxed) || !q.empty(); });
                        if (stop.load(std::memory_order_relaxed) && q.empty())
                        {
                            return;
                        }
                        job = std::move(q.front());
                        q.pop();
                    }
                    job();
                }
            });
        }
    }

    ~ThreadPool()
    {
        stop.store(true, std::memory_order_relaxed);
        cv.notify_all();
        for (auto& w : workers)
        {
            if (w.joinable())
            {
                w.join();
            }
        }
    }

    template <typename F>
    void Enqueue(F&& f)
    {
        {
            std::lock_guard<std::mutex> lk(m);
            q.emplace(std::forward<F>(f));
        }
        cv.notify_one();
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> q;
    std::mutex m;
    std::condition_variable cv;
    std::atomic_bool stop;
};

struct DirEntryView
{
    fs::path path;
    bool is_dir = false;
    std::string name;
};

struct VortexDirectory
{
    fs::path path;
    std::vector<DirEntryView> items;

    std::vector<std::size_t> FilteredIndices(std::string const& q, bool find_mode) const
    {
        std::vector<std::size_t> out;
        out.reserve(items.size());
        if (!find_mode && q.empty())
        {
            out.resize(items.size());
            for (std::size_t i = 0; i < items.size(); ++i)
            {
                out[i] = i;
            }
            return out;
        }
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if (CaseInsensitiveContains(items[i].name, q))
            {
                out.push_back(i);
            }
        }
        return out;
    }
};

static fs::path SafeParentPath(fs::path const& p)
{
    fs::path parent = p.parent_path();
    return parent.empty() ? p : parent;
}

struct VortexAppState
{
    VortexDirectory parent_dir;
    VortexDirectory current_dir;
    std::atomic<std::uint64_t> generation{0};
    std::atomic_bool size_cancel{false};
    std::mutex size_m;
    std::unordered_map<std::string, std::uintmax_t> cached_sizes;
    ThreadPool pool;
    bool find_mode = false;
    std::string find_query;
    std::string action_input_buffer;
    std::size_t left_selected = 0;
    std::size_t mid_selected = 0;

    enum class FocusTarget
    {
        Left,
        Middle,
        Right,
        ActionInput
    };

    FocusTarget focus = FocusTarget::Middle;
    std::mutex fs_m;
    std::function<void()> refresh_ui_cb;

    explicit VortexAppState(std::size_t threads) : pool(threads)
    {
    }

    void LoadDirectory(VortexDirectory& dir, fs::path const& p)
    {
        dir.path = p;
        dir.items.clear();
        std::error_code ec;
        std::vector<fs::directory_entry> temp;
        for (fs::directory_entry const& e : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            temp.push_back(e);
        }
        std::sort(temp.begin(), temp.end(), [](fs::directory_entry const& a, fs::directory_entry const& b)
        {
            bool ad = IsDirectoryFast(a);
            bool bd = IsDirectoryFast(b);
            if (ad != bd)
            {
                return ad > bd;
            }
            return a.path().filename().string() < b.path().filename().string();
        });
        dir.items.reserve(temp.size());
        for (auto const& e : temp)
        {
            DirEntryView v;
            v.path = e.path();
            v.is_dir = IsDirectoryFast(e);
            v.name = e.path().filename().string();
            dir.items.push_back(std::move(v));
        }
    }

    void RefreshAll()
    {
        std::lock_guard<std::mutex> lk(fs_m);
        fs::path current = current_dir.path.empty() ? fs::current_path() : current_dir.path;
        LoadDirectory(current_dir, current);
        fs::path up = SafeParentPath(current);
        if (up == current)
        {
            parent_dir = VortexDirectory{};
            parent_dir.path = current;
        }
        else
        {
            LoadDirectory(parent_dir, up);
        }
    }

    bool CanNavigateUp() const
    {
        return current_dir.path.has_parent_path() && current_dir.path != current_dir.path.root_path();
    }

    void NavigateUp()
    {
        if (!CanNavigateUp())
        {
            return;
        }
        fs::path up = SafeParentPath(current_dir.path);
        {
            std::lock_guard<std::mutex> lk(fs_m);
            current_dir.path = up;
        }
        RefreshAll();
        StartAsyncSizes();
        left_selected = 0;
        mid_selected = 0;
        focus = FocusTarget::Middle;
    }

    void EnterMiddle(std::size_t index)
    {
        if (index >= current_dir.items.size())
        {
            return;
        }
        if (!current_dir.items[index].is_dir)
        {
            return;
        }
        {
            std::lock_guard<std::mutex> lk(fs_m);
            current_dir.path = current_dir.items[index].path;
        }
        RefreshAll();
        StartAsyncSizes();
        mid_selected = 0;
        left_selected = 0;
        focus = FocusTarget::Middle;
    }

    void OpenEntry(fs::path const& p)
    {
#ifdef _WIN32
        (void)p;
#else
        std::string command = std::string("xdg-open '") + p.string() + "' >/dev/null 2>&1 &";
        (void)std::thread([command]() { (void)std::system(command.c_str()); }).detach();
#endif
    }

    void CreateFileFromName(std::string const& name)
    {
        std::string n = name;
        if (n.empty())
        {
            n = "NewFile.txt";
        }
        fs::path target;
        {
            std::lock_guard<std::mutex> lk(fs_m);
            target = current_dir.path / n;
        }
        std::ofstream ofs(target.string(), std::ios::out | std::ios::trunc);
        ofs.close();
        RefreshAll();
        StartAsyncSizes();
    }

    void DeleteEntry(fs::path const& p)
    {
        std::error_code ec;
        fs::remove_all(p, ec);
        RefreshAll();
        StartAsyncSizes();
    }

    void RenameEntry(fs::path const& oldp, std::string const& new_name)
    {
        if (new_name.empty())
        {
            return;
        }
        fs::path newp;
        {
            std::lock_guard<std::mutex> lk(fs_m);
            newp = current_dir.path / new_name;
        }
        std::error_code ec;
        fs::rename(oldp, newp, ec);
        RefreshAll();
        StartAsyncSizes();
    }

    void StartAsyncSizes()
    {
        size_cancel.store(true, std::memory_order_relaxed);
        size_cancel.store(false, std::memory_order_relaxed);
        std::uint64_t gen = generation.fetch_add(1, std::memory_order_relaxed) + 1;
        std::vector<fs::path> targets;
        {
            std::lock_guard<std::mutex> lk(fs_m);
            targets.reserve(current_dir.items.size());
            for (auto const& item : current_dir.items)
            {
                if (item.is_dir)
                {
                    targets.push_back(item.path);
                }
            }
        }
        for (auto const& p : targets)
        {
            pool.Enqueue([this, p, gen]()
            {
                if (generation.load(std::memory_order_relaxed) != gen)
                {
                    return;
                }
                std::atomic_bool& cancel = size_cancel;
                std::uintmax_t sz = ComputeDirectorySizeRecursive(p, cancel);
                if (generation.load(std::memory_order_relaxed) != gen)
                {
                    return;
                }
                std::string key = p.string();
                {
                    std::lock_guard<std::mutex> lk(size_m);
                    cached_sizes[key] = sz;
                }
                if (refresh_ui_cb)
                {
                    refresh_ui_cb();
                }
            });
        }
    }

    std::optional<std::uintmax_t> SizeFor(fs::path const& p)
    {
        std::string key = p.string();
        std::lock_guard<std::mutex> lk(size_m);
        auto it = cached_sizes.find(key);
        if (it == cached_sizes.end())
        {
            return std::nullopt;
        }
        return it->second;
    }
};

int main()
{
    auto screen = ScreenInteractive::Fullscreen();
    std::size_t threads = 4;
    auto state = std::make_shared<VortexAppState>(threads);
    state->refresh_ui_cb = [&screen]() { screen.PostEvent(Event::Custom); };
    {
        state->current_dir.path = fs::current_path();
        state->RefreshAll();
        state->StartAsyncSizes();
    }

    auto action_input_comp = Input(&state->action_input_buffer, "rename/create");

    auto render_files = [&](VortexDirectory const& dir, std::size_t selected, bool show_up_entry) -> Element
    {
        std::vector<std::size_t> filtered = dir.FilteredIndices(state->find_query, state->find_mode);
        Elements rows;
        auto push_cell = [&](bool sel, std::string const& label)
        {
            auto cell = text(label);
            if (sel)
            {
                cell = text(label) | color(Color::Black) | bgcolor(Color::BlueLight) | ftxui::focus;
            }
            else
            {
                cell = text(label) | color(Color::White);
            }
            rows.push_back(hbox(cell));
        };
        if (show_up_entry)
        {
            bool sel = (selected == 0);
            push_cell(sel, std::string("📁 .."));
        }
        std::size_t offset = show_up_entry ? 1 : 0;
        for (std::size_t k = 0; k < filtered.size(); ++k)
        {
            std::size_t idx = filtered[k];
            std::string label;
            if (dir.items[idx].is_dir)
            {
                label = std::string("📁 ") + dir.items[idx].name;
            }
            else
            {
                label = std::string("📄 ") + dir.items[idx].name;
            }
            bool sel = (selected == k + offset);
            push_cell(sel, label);
        }
        auto title = text(show_up_entry ? " Current " : " Parent ") | bold;
        if (state->find_mode && state->focus != VortexAppState::FocusTarget::ActionInput)
        {
            title = text(" Find ") | bold | color(Color::Green);
        }
        auto list = vbox(std::move(rows)) | vscroll_indicator | frame | flex;
        return vbox({title, separator(), list});
    };

    auto main_renderer = Renderer(action_input_comp, [&]()
    {
        std::vector<std::size_t> left_filtered = state->parent_dir.FilteredIndices(state->find_query, state->find_mode);
        std::vector<std::size_t> mid_filtered = state->current_dir.FilteredIndices(state->find_query, state->find_mode);
        if (state->left_selected >= left_filtered.size())
        {
            state->left_selected = left_filtered.empty() ? 0 : left_filtered.size() - 1;
        }
        std::size_t total_mid = mid_filtered.size() + 1;
        if (state->mid_selected >= total_mid)
        {
            state->mid_selected = total_mid - 1;
        }
        DirEntryView const* right_item = nullptr;
        if (!mid_filtered.empty() && state->mid_selected > 0)
        {
            std::size_t k = state->mid_selected - 1;
            if (k < mid_filtered.size())
            {
                std::size_t right_index = mid_filtered[k];
                right_item = &state->current_dir.items[right_index];
            }
        }
        std::string right_title = " Preview ";
        Color border_color = Color::Default;
        if (state->focus == VortexAppState::FocusTarget::ActionInput)
        {
            border_color = Color::Blue;
        }
        else if (state->find_mode)
        {
            border_color = Color::Green;
            right_title = " Find Mode ";
        }
        std::string preview_path;
        std::string preview_type;
        std::string preview_size;
        std::string preview_meta;
        if (right_item)
        {
            preview_path = right_item->path.string();
            preview_type = right_item->is_dir ? "Directory" : "File";
            if (right_item->is_dir)
            {
                auto sz = state->SizeFor(right_item->path);
                preview_size = sz ? FormatBytes(*sz) : "Calculating...";
            }
            else
            {
                std::error_code ec;
                std::uintmax_t file_sz = fs::file_size(right_item->path, ec);
                preview_size = ec ? "Unknown" : FormatBytes(file_sz);
            }
            preview_meta = right_item->is_dir ? "Size computed async" : "Instant size";
        }
        else
        {
            preview_path = state->current_dir.path.string();
            preview_type = "Parent / No selection";
            preview_size = "-";
            preview_meta = "Navigate current column";
        }
        auto meta_box = vbox({
            text(right_title) | bold | color(border_color),
            separator(),
            text("Type: " + preview_type) | color(Color::White),
            text("Size: " + preview_size) | color(Color::White),
            separator(),
            text(preview_meta) | color(Color::Default),
            separator(),
            text("Path: " + preview_path) | color(Color::White) | flex
        });
        auto right_panel = vbox({meta_box}) | border | color(border_color) | size(WIDTH, EQUAL, 38);
        auto left_frame = render_files(state->parent_dir, state->left_selected, false);
        auto mid_frame = render_files(state->current_dir, state->mid_selected, true);
        if (state->focus == VortexAppState::FocusTarget::Left)
        {
            left_frame = left_frame | border | color(Color::Blue);
        }
        else
        {
            left_frame = left_frame | border;
        }
        if (state->focus == VortexAppState::FocusTarget::Middle)
        {
            mid_frame = mid_frame | border | color(Color::Blue);
        }
        else
        {
            mid_frame = mid_frame | border;
        }
        auto hint = [&]()
        {
            if (state->focus == VortexAppState::FocusTarget::Left)
            {
                return std::string("Focus: Parent | [->] Center current | [Tab] Input");
            }
            if (state->focus == VortexAppState::FocusTarget::Middle)
            {
                return std::string("Focus: Current | [->] Enter Directory / Open File | [<-] Go Parent");
            }
            if (state->focus == VortexAppState::FocusTarget::Right)
            {
                return std::string("Focus: Preview Panel | [<-] Return to Current");
            }
            return std::string("Focus: Action Input | [Esc] Cancel | [Enter] Apply");
        }();
        auto shortcuts_panel = vbox({
            text(" ShortCuts ") | bold,
            separator(),
            hbox({
                vbox({
                    text(" [->]   Enter/Open Selected"),
                    text(" [<-]   Navigate to Parent"),
                    text(" [F]    Find Mode"),
                    text(" [R]    Rename Entry")
                }),
                separator(),
                vbox({
                    text(" [N]    Create File"),
                    text(" [Del]  Delete Selected"),
                    text(" [Esc]  Cancel Action"),
                    text(" [Q]    Quit Application")
                })
            })
        }) | border | color(Color::Default);
        auto action_title = state->find_mode ? "Rename/Create (Find active)" : "Rename/Create Input";
        auto action_window = vbox({
            text(action_title) | bold,
            separator(),
            action_input_comp->Render()
        }) | border | size(HEIGHT, EQUAL, 5);
        if (state->focus == VortexAppState::FocusTarget::ActionInput)
        {
            action_window = action_window | color(Color::Blue);
        }
        auto bottom = vbox({separator(), text(hint) | color(Color::Default)}) | size(HEIGHT, EQUAL, 2);
        return vbox({
            hbox({
                left_frame | flex,
                mid_frame | flex,
                right_panel
            }) | flex,
            action_window,
            shortcuts_panel,
            bottom
        });
    });

    auto event_handler = CatchEvent(main_renderer, [&](Event event)
    {
        if (event == Event::Character('q') || event == Event::Character('Q'))
        {
            if (state->focus != VortexAppState::FocusTarget::ActionInput)
            {
                screen.ExitLoopClosure()();
                return true;
            }
        }
        if (event == Event::Tab)
        {
            using FT = VortexAppState::FocusTarget;
            if (state->focus == FT::Left)
            {
                state->focus = FT::Middle;
            }
            else if (state->focus == FT::Middle)
            {
                state->focus = FT::Right;
            }
            else if (state->focus == FT::Right)
            {
                state->focus = FT::ActionInput;
            }
            else
            {
                state->focus = FT::Left;
            }
            return true;
        }
        if (state->focus != VortexAppState::FocusTarget::ActionInput)
        {
            if (event == Event::Character('f') || event == Event::Character('F'))
            {
                state->find_mode = true;
                state->find_query.clear();
                state->action_input_buffer.clear();
                state->focus = VortexAppState::FocusTarget::ActionInput;
                return true;
            }
            if (event == Event::Character('r') || event == Event::Character('R'))
            {
                if (state->focus == VortexAppState::FocusTarget::Middle && state->mid_selected > 0)
                {
                    state->find_mode = false;
                    state->action_input_buffer.clear();
                    state->focus = VortexAppState::FocusTarget::ActionInput;
                    return true;
                }
            }
        }
        if (event == Event::Escape)
        {
            state->find_mode = false;
            state->find_query.clear();
            state->action_input_buffer.clear();
            state->focus = VortexAppState::FocusTarget::Middle;
            return true;
        }
        using FT = VortexAppState::FocusTarget;
        if (state->focus == FT::ActionInput)
        {
            if (state->find_mode)
            {
                if (event == Event::Return)
                {
                    state->find_mode = false;
                    state->focus = FT::Middle;
                    return true;
                }
                bool handled = action_input_comp->OnEvent(event);
                state->find_query = state->action_input_buffer;
                return handled;
            }
            if (event == Event::Return)
            {
                state->focus = FT::Middle;
                if (state->mid_selected == 0)
                {
                    state->action_input_buffer.clear();
                    return true;
                }
                std::vector<std::size_t> filtered = state->current_dir.FilteredIndices(state->find_query, false);
                if (state->mid_selected == 0 || (state->mid_selected - 1) >= filtered.size())
                {
                    state->action_input_buffer.clear();
                    return true;
                }
                std::size_t real_idx = filtered[state->mid_selected - 1];
                fs::path oldp = state->current_dir.items[real_idx].path;
                state->RenameEntry(oldp, state->action_input_buffer);
                state->action_input_buffer.clear();
                return true;
            }
            return action_input_comp->OnEvent(event);
        }
        
        if (event == Event::ArrowLeft)
        {
            state->NavigateUp();
            return true;
        }

        if (state->focus == FT::Left)
        {
            std::vector<std::size_t> filtered = state->parent_dir.FilteredIndices(state->find_query, state->find_mode);
            if (event == Event::ArrowDown)
            {
                if (!filtered.empty() && state->left_selected + 1 < filtered.size())
                {
                    state->left_selected++;
                }
                return true;
            }
            if (event == Event::ArrowUp)
            {
                if (state->left_selected > 0)
                {
                    state->left_selected--;
                }
                return true;
            }
            if (event == Event::ArrowRight || event == Event::Return)
            {
                state->focus = FT::Middle;
                return true;
            }
            return false;
        }
        if (state->focus == FT::Middle)
        {
            std::vector<std::size_t> filtered = state->current_dir.FilteredIndices(state->find_query, state->find_mode);
            std::size_t total = filtered.size() + 1;
            if (event == Event::ArrowDown)
            {
                if (state->mid_selected + 1 < total)
                {
                    state->mid_selected++;
                }
                return true;
            }
            if (event == Event::ArrowUp)
            {
                if (state->mid_selected > 0)
                {
                    state->mid_selected--;
                }
                return true;
            }
            if (event == Event::ArrowRight || event == Event::Return)
            {
                if (state->mid_selected == 0)
                {
                    state->NavigateUp();
                    return true;
                }
                std::size_t k = state->mid_selected - 1;
                if (k >= filtered.size())
                {
                    return true;
                }
                std::size_t real_idx = filtered[k];
                auto& item = state->current_dir.items[real_idx];
                if (item.is_dir)
                {
                    state->EnterMiddle(real_idx);
                }
                else
                {
                    state->OpenEntry(item.path);
                }
                return true;
            }
            if (event == Event::Character('n') || event == Event::Character('N'))
            {
                state->CreateFileFromName(state->action_input_buffer);
                state->action_input_buffer.clear();
                return true;
            }
            if (event == Event::Delete)
            {
                if (state->mid_selected == 0)
                {
                    return true;
                }
                std::size_t k = state->mid_selected - 1;
                if (k < filtered.size())
                {
                    fs::path p = state->current_dir.items[filtered[k]].path;
                    state->DeleteEntry(p);
                }
                return true;
            }
            return false;
        }
        if (state->focus == FT::Right)
        {
            if (event == Event::Return)
            {
                std::vector<std::size_t> filtered = state->current_dir.FilteredIndices(state->find_query, state->find_mode);
                if (state->mid_selected == 0)
                {
                    state->NavigateUp();
                    return true;
                }
                std::size_t k = state->mid_selected - 1;
                if (k < filtered.size())
                {
                    fs::path p = state->current_dir.items[filtered[k]].path;
                    state->OpenEntry(p);
                }
                return true;
            }
            return false;
        }
        return false;
    });

    screen.Loop(event_handler);
    return 0;
}
```
