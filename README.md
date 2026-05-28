# VORTEX (Visual Object Routing & Terminal File Explorer)

VORTEX is a high-performance, interactive Terminal User Interface (TUI) file manager and directory router built in modern C++. Powered by the functional FTXUI library and standard filesystem vectors, it provides a lightning-fast, keyboard-driven navigation dashboard with zero dependency bloating.

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
VORTEX strictly obeys the foundational principles of minimized structural footprints. It encapsulates core state managers within isolated component render scopes, ensuring consistent tick performance even under high-density system structures. Part of the automated development toolsets pipeline.

---

## Fun Facts!
- This project took 4 months from me! There were 3 main attempts for this project.
1. Java + JavaFX + Fortran + C++ + C++ Bridge
 After this attempt, I stopped the project VorteX for about 2 months. I do not think that i should tell the reason it was terminated, but whatever. The reaseon is that all these modules could not communicate efficiently and JavaFX was not good for VorteX.
2. C++ CLI
 This one did not look pretty but more importantly, it did not work well. It had a lot of bugs. After this attempt, I stopped project VorteX again for approximately 1 month.
3. C++ FTXUI (TUI)
 This was the true final of the project VorteX. It looked so pretty and worked well without bugs.

- VorteX is inspired by (sxyazi/yazi)[https://github.com/sxyazi/yazi].

- VorteX has a one-line edition source code in my other repository (OneLineHub)[https://github.com/hypernova-developer/OneLineHub]. 470 lines, compressed to ***one***!

---

## Appendix: Master Source Artifact

```cpp
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>
#include <system_error>

using namespace ftxui;
namespace fs = std::filesystem;

struct VortexManager
{
    fs::path current_directory;
    std::vector<fs::directory_entry> entries;
    std::vector<std::string> names;

    void SetInitialDirectory()
    {
        current_directory = fs::current_path();
        Refresh();
    }

    void Refresh()
    {
        entries.clear();
        names.clear();
        std::vector<fs::directory_entry> temp;
        std::error_code ec;
        for (fs::directory_entry const& e : fs::directory_iterator(current_directory, fs::directory_options::skip_permission_denied, ec))
        {
            (void)ec;
            temp.push_back(e);
        }
        std::sort(temp.begin(), temp.end(), [](fs::directory_entry const& a, fs::directory_entry const& b)
        {
            bool ad = a.is_directory();
            bool bd = b.is_directory();
            if (ad != bd)
            {
                return ad > bd;
            }
            return a.path().filename().string() < b.path().filename().string();
        });
        entries = std::move(temp);
        for (auto const& e : entries)
        {
            names.push_back(e.path().filename().string());
        }
    }

    void EnterDirectory(size_t index)
    {
        if (index >= entries.size())
        {
            return;
        }
        if (!entries[index].is_directory())
        {
            return;
        }
        current_directory = entries[index].path();
        Refresh();
    }

    bool CanNavigateUp() const
    {
        return current_directory.has_parent_path();
    }

    void NavigateUp()
    {
        if (!CanNavigateUp())
        {
            return;
        }
        current_directory = current_directory.parent_path();
        Refresh();
    }

    void OpenEntry(size_t index) const
    {
        if (index >= entries.size())
        {
            return;
        }
        fs::path p = entries[index].path();
        std::string command = std::string("xdg-open ") + "'" + p.string() + "'" + " >/dev/null 2>&1 &";
        (void)std::thread([command]()
        {
            (void)std::system(command.c_str());
        }).detach();
    }

    void CreateFileFromName(std::string const& name)
    {
        std::string n = name;
        if (n.empty())
        {
            n = "NewFile.txt";
        }
        fs::path target = current_directory / n;
        std::ofstream ofs(target.string(), std::ios::out | std::ios::trunc);
        ofs.close();
        Refresh();
    }

    void DeleteEntry(size_t index)
    {
        if (index >= entries.size())
        {
            return;
        }
        std::error_code ec;
        fs::remove_all(entries[index].path(), ec);
        Refresh();
    }

    void RenameEntry(size_t index, std::string const& new_name)
    {
        if (index >= entries.size())
        {
            return;
        }
        if (new_name.empty())
        {
            return;
        }
        fs::path oldp = entries[index].path();
        fs::path newp = current_directory / new_name;
        std::error_code ec;
        fs::rename(oldp, newp, ec);
        Refresh();
    }
};

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

int main()
{
    auto screen = ScreenInteractive::Fullscreen();
    VortexManager manager;
    manager.SetInitialDirectory();

    bool find_mode = false;
    std::string find_query;
    std::string action_input_buffer;
    size_t selected_entry = 0;

    enum class FocusTarget
    {
        FileList,
        ActionInput
    };
    FocusTarget focus = FocusTarget::FileList;

    auto ComputeFiltered = [&]()
    {
        std::vector<size_t> filtered;
        filtered.reserve(manager.entries.size());
        for (size_t i = 0; i < manager.entries.size(); ++i)
        {
            std::string const& name = manager.entries[i].path().filename().string();
            if (!find_mode && find_query.empty())
            {
                filtered.push_back(i);
            }
            else
            {
                if (CaseInsensitiveContains(name, find_query))
                {
                    filtered.push_back(i);
                }
            }
        }
        return filtered;
    };

    auto shortcuts_panel = [&]()
    {
        Elements rows;
        rows.push_back(hbox({text("Tab"), separator(), text("Switch Focus")}));
        rows.push_back(hbox({text("F"), separator(), text("Find (File List)")}));
        rows.push_back(hbox({text("Esc"), separator(), text("Cancel Find / Focus Left")}));
        rows.push_back(hbox({text("Enter"), separator(), text("Open / Confirm")}));
        rows.push_back(hbox({text("N"), separator(), text("Create File from Action Input")}));
        rows.push_back(hbox({text("Del"), separator(), text("Delete Selected")}));
        rows.push_back(hbox({text("Q"), separator(), text("Quit")}));
        return vbox(std::move(rows)) | size(WIDTH, EQUAL, 38);
    };

    bool quitting = false;

    auto action_input_comp = Input(&action_input_buffer, "Type here...");

    auto main_renderer = Renderer(action_input_comp, [&]()
    {
        std::vector<size_t> filtered = ComputeFiltered();
        
        size_t total_items = filtered.size() + 1; 
        if (selected_entry >= total_items)
        {
            selected_entry = total_items - 1;
        }

        Elements file_items;
        bool is_filelist = (focus == FocusTarget::FileList);

        {
            bool selected = is_filelist && (selected_entry == 0);
            auto cell = text("📁 ..") | color(Color::White);
            if (selected)
            {
                cell = text("📁 ..") | color(Color::Black) | bgcolor(Color::BlueLight) | ftxui::focus;
            }
            file_items.push_back(hbox({cell}));
        }

        for (size_t k = 0; k < filtered.size(); ++k)
        {
            size_t original_index = filtered[k];
            bool is_dir = manager.entries[original_index].is_directory();
            std::string label = (is_dir ? "📁 " : "📄 ") + manager.entries[original_index].path().filename().string();
            bool selected = is_filelist && (k + 1 == selected_entry);
            auto cell = text(label) | color(Color::White);
            if (selected)
            {
                cell = text(label) | color(Color::Black) | bgcolor(Color::BlueLight) | ftxui::focus;
            }
            file_items.push_back(hbox({cell}));
        }

        auto files_title = text(" Files [Tab to switch] ") | bold;
        auto files_window = vbox({
            files_title,
            separator(),
            vbox(std::move(file_items)) | vscroll_indicator | frame | flex
        });

        if (is_filelist)
        {
            files_window = files_window | border | color(Color::Blue);
        }
        else
        {
            files_window = files_window | border | color(Color::Default);
        }

        std::string right_title = " Action Input (Rename/Create) ";
        Color border_color = Color::Default;

        if (find_mode)
        {
            right_title = " Find Mode (Typing...) ";
            border_color = Color::Green;
        }
        else if (focus == FocusTarget::ActionInput)
        {
            border_color = Color::Blue;
        }

        auto action_window = vbox({
            text(right_title) | bold | color(border_color),
            separator(),
            action_input_comp->Render()
        }) | border | color(border_color) | size(HEIGHT, EQUAL, 3);

        auto shortcuts_window = vbox({
            text(" Shortcuts ") | bold,
            separator(),
            shortcuts_panel()
        }) | border | color(Color::Default) | flex;

        auto right_panel = vbox({
            action_window,
            shortcuts_window
        }) | size(WIDTH, EQUAL, 40);

        auto hint_prefix = find_mode ? "Find: " + find_query : "Path: " + manager.current_directory.string();

        return vbox({
            hbox({
                files_window | flex,
                right_panel
            }) | flex,
            vbox({
                separator(),
                text(hint_prefix)
            }) | size(HEIGHT, EQUAL, 2)
        });
    });

    auto event_handler = CatchEvent(main_renderer, [&](Event event)
    {
        if (quitting)
        {
            return true;
        }

        if (focus == FocusTarget::ActionInput)
        {
            if (event == Event::Escape)
            {
                find_mode = false;
                find_query.clear();
                action_input_buffer.clear();
                focus = FocusTarget::FileList;
                return true;
            }

            if (event == Event::Return)
            {
                if (find_mode)
                {
                    find_mode = false;
                    focus = FocusTarget::FileList;
                    return true;
                }
                else
                {
                    std::vector<size_t> filtered = ComputeFiltered();
                    if (!filtered.empty() && selected_entry != 0)
                    {
                        size_t real_idx = filtered[selected_entry - 1];
                        manager.RenameEntry(real_idx, action_input_buffer);
                        action_input_buffer.clear();
                        focus = FocusTarget::FileList;
                    }
                    return true;
                }
            }

            if (find_mode && event.is_character())
            {
                action_input_comp->OnEvent(event);
                find_query = action_input_buffer;
                return true;
            }

            if (find_mode && event == Event::Backspace)
            {
                action_input_comp->OnEvent(event);
                find_query = action_input_buffer;
                return true;
            }

            return false;
        }
        else
        {
            if (event == Event::Tab)
            {
                focus = FocusTarget::ActionInput;
                return true;
            }

            if (event == Event::Character('f') || event == Event::Character('F'))
            {
                find_mode = true;
                find_query.clear();
                action_input_buffer.clear();
                focus = FocusTarget::ActionInput;
                return true;
            }

            if (event == Event::Character('q') || event == Event::Character('Q'))
            {
                quitting = true;
                screen.ExitLoopClosure()();
                return true;
            }

            if (event == Event::ArrowDown)
            {
                std::vector<size_t> filtered = ComputeFiltered();
                if (selected_entry < filtered.size())
                {
                    selected_entry++;
                }
                return true;
            }

            if (event == Event::ArrowUp)
            {
                if (selected_entry > 0)
                {
                    selected_entry--;
                }
                return true;
            }

            if (event == Event::Return)
            {
                std::vector<size_t> filtered = ComputeFiltered();
                if (selected_entry == 0)
                {
                    manager.NavigateUp();
                    return true;
                }
                if (!filtered.empty() && (selected_entry - 1) < filtered.size())
                {
                    size_t real_idx = filtered[selected_entry - 1];
                    if (manager.entries[real_idx].is_directory())
                    {
                        manager.EnterDirectory(real_idx);
                        selected_entry = 0;
                    }
                    else
                    {
                        manager.OpenEntry(real_idx);
                    }
                }
                return true;
            }

            if (event == Event::Character('n') || event == Event::Character('N'))
            {
                manager.CreateFileFromName(action_input_buffer);
                action_input_buffer.clear();
                return true;
            }

            if (event == Event::Delete)
            {
                std::vector<size_t> filtered = ComputeFiltered();
                if (!filtered.empty() && selected_entry != 0)
                {
                    size_t real_idx = filtered[selected_entry - 1];
                    manager.DeleteEntry(real_idx);
                    if (selected_entry > filtered.size() - 1)
                    {
                        selected_entry = filtered.size() - 1;
                    }
                }
                return true;
            }
        }
        return false;
    });

    screen.Loop(event_handler);
    return 0;
}
```
