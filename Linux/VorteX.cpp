#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace fs = std::filesystem;
using namespace ftxui;

struct FileItem {
    std::string name;
    bool isDirectory;
    bool isParent;
    std::string sizeStr;
};

class VortexManager {
private:
    fs::path currentPath;
    std::vector<FileItem> currentFiles;
    int selectedIndex;

public:
    VortexManager() { currentPath = fs::current_path(); selectedIndex = 0; refreshDirectory(); }

    void refreshDirectory() {
        currentFiles.clear();
        currentFiles.push_back({"..", true, true, "<DIR>"});
        try {
            for (const auto& entry : fs::directory_iterator(currentPath)) {
                FileItem item;
                item.name = entry.path().filename().string();
                item.isDirectory = entry.is_directory();
                item.isParent = false;
                item.sizeStr = item.isDirectory ? "<DIR>" : std::to_string(entry.file_size() / 1024) + " KB";
                currentFiles.push_back(item);
            }
        } catch (...) {}
    }

    void openFile() {
        auto& item = currentFiles[selectedIndex];
        if (item.isParent) { currentPath = currentPath.parent_path(); refreshDirectory(); selectedIndex = 0; }
        else if (item.isDirectory) { currentPath /= item.name; refreshDirectory(); selectedIndex = 0; }
        else { std::string cmd = "xdg-open \"" + (currentPath / item.name).string() + "\""; std::system(cmd.c_str()); }
    }

    void deleteFile() {
        if (currentFiles[selectedIndex].isParent) return;
        std::string cmd = "rm -rf \"" + (currentPath / currentFiles[selectedIndex].name).string() + "\"";
        std::system(cmd.c_str());
        refreshDirectory();
    }

    void editFile() {
        if (currentFiles[selectedIndex].isDirectory || currentFiles[selectedIndex].isParent) return;
        std::string cmd = "nano \"" + (currentPath / currentFiles[selectedIndex].name).string() + "\"";
        std::system(cmd.c_str());
    }

    void renameFile() {
        if (currentFiles[selectedIndex].isParent) return;
        std::cout << "\033[2J\033[1;1H";
        std::cout << "Enter new name for " << currentFiles[selectedIndex].name << ": ";
        std::string newName; std::cin >> newName;
        fs::rename(currentPath / currentFiles[selectedIndex].name, currentPath / newName);
        refreshDirectory();
    }

    void moveUp() { if (selectedIndex > 0) selectedIndex--; }
    void moveDown() { if (selectedIndex < (int)currentFiles.size() - 1) selectedIndex++; }

    Element renderUI() {
        Elements listElements;
        for (int i = 0; i < (int)currentFiles.size(); ++i) {
            bool isSelected = (i == selectedIndex);
            listElements.push_back(text((isSelected ? "> " : "  ") + currentFiles[i].name) | (isSelected ? bgcolor(Color::Blue) : nothing));
        }
        return vbox({
            window(text(" Vortex "), vbox(std::move(listElements))) | flex,
            text(" [Enter]: Open | [Delete]: Del | [E]: Edit | [R]: Rename | [Q]: Quit ") | center
        });
    }
};

int main() {
    auto screen = ScreenInteractive::Fullscreen();
    VortexManager vortex;
    auto component = CatchEvent(Renderer([&] { return vortex.renderUI(); }), [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q') || event == Event::Escape) screen.ExitLoopClosure()();
        else if (event == Event::ArrowUp) vortex.moveUp();
        else if (event == Event::ArrowDown) vortex.moveDown();
        else if (event == Event::Return) vortex.openFile();
        else if (event == Event::Character('e') || event == Event::Character('E')) vortex.editFile();
        else if (event == Event::Character('r') || event == Event::Character('R')) vortex.renameFile();
        else if (event == Event::Special(Key::Delete)) vortex.deleteFile();
        else return false;
        return true;
    });
    screen.Loop(component);
    return 0;
}

/*#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>

namespace fs = std::filesystem;
using namespace ftxui;

struct FileItem
{
    std::string name;
    bool isDirectory;
    std::string sizeStr;
    std::string permissions;
};

class VortexManager
{
private:
    fs::path currentPath;
    std::vector<FileItem> currentFiles;
    int selectedIndex;

    std::string formatSize(uintmax_t size)
    {
        if (size == static_cast<uintmax_t>(-1)) return "<DIR>";
        if (size < 1024) return std::to_string(size) + " B";
        if (size < 1024 * 1024) return std::to_string(size / 1024) + " KB";
        return std::to_string(size / (1024 * 1024)) + " MB";
    }

    std::string getPermissionsStr(fs::perms p)
    {
        std::string res = "---------";
        if ((p & fs::perms::owner_read) != fs::perms::none) res[0] = 'r';
        if ((p & fs::perms::owner_write) != fs::perms::none) res[1] = 'w';
        if ((p & fs::perms::owner_exec) != fs::perms::none) res[2] = 'x';
        if ((p & fs::perms::group_read) != fs::perms::none) res[3] = 'r';
        if ((p & fs::perms::group_write) != fs::perms::none) res[4] = 'w';
        if ((p & fs::perms::group_exec) != fs::perms::none) res[5] = 'x';
        if ((p & fs::perms::others_read) != fs::perms::none) res[6] = 'r';
        if ((p & fs::perms::others_write) != fs::perms::none) res[7] = 'w';
        if ((p & fs::perms::others_exec) != fs::perms::none) res[8] = 'x';
        return res;
    }

public:
    VortexManager()
    {
        currentPath = fs::current_path();
        selectedIndex = 0;
        refreshDirectory();
    }

    void refreshDirectory()
    {
        currentFiles.clear();
        try
        {
            std::vector<FileItem> dirs;
            std::vector<FileItem> files;
            for (const auto& entry : fs::directory_iterator(currentPath))
            {
                FileItem item;
                item.name = entry.path().filename().string();
                item.isDirectory = entry.is_directory();
                item.sizeStr = item.isDirectory ? "<DIR>" : formatSize(entry.file_size());
                try { item.permissions = getPermissionsStr(entry.status().permissions()); }
                catch (...) { item.permissions = "---"; }
                
                if (item.isDirectory) dirs.push_back(item);
                else files.push_back(item);
            }
            auto comp = [](const FileItem& a, const FileItem& b) { return a.name < b.name; };
            std::sort(dirs.begin(), dirs.end(), comp);
            std::sort(files.begin(), files.end(), comp);
            currentFiles.insert(currentFiles.end(), dirs.begin(), dirs.end());
            currentFiles.insert(currentFiles.end(), files.begin(), files.end());
        } catch (...) {}

        if (selectedIndex >= (int)currentFiles.size())
            selectedIndex = std::max(0, (int)currentFiles.size() - 1);
    }

    void moveUp() { if (selectedIndex > 0) selectedIndex--; }
    void moveDown() { if (selectedIndex < (int)currentFiles.size() - 1) selectedIndex++; }
    
    void enterDirectory()
    {
        if (currentFiles.empty()) return;
        if (currentFiles[selectedIndex].isDirectory)
        {
            currentPath /= currentFiles[selectedIndex].name;
            selectedIndex = 0;
            refreshDirectory();
        }
    }

    void leaveDirectory()
    {
        if (currentPath.has_parent_path())
        {
            currentPath = currentPath.parent_path();
            selectedIndex = 0;
            refreshDirectory();
        }
    }

    Element renderUI()
    {
        Elements listElements;
        if (currentFiles.empty())
        {
            listElements.push_back(text(" Empty Directory ") | dim);
        }
        else
        {
            for (int i = 0; i < (int)currentFiles.size(); ++i)
            {
                std::string textLine = (currentFiles[i].isDirectory ? "📁 " : "📄 ") + currentFiles[i].name;
                if (i == selectedIndex) listElements.push_back(text(textLine) | bold | bgcolor(Color::Blue));
                else listElements.push_back(text(textLine));
            }
        }

        Element previewBox = window(text(" Details "), vbox({
            text("Name: " + (currentFiles.empty() ? "None" : currentFiles[selectedIndex].name)),
            text("Size: " + (currentFiles.empty() ? "N/A" : currentFiles[selectedIndex].sizeStr)),
            text("Perms: " + (currentFiles.empty() ? "N/A" : currentFiles[selectedIndex].permissions))
        }));

        return vbox({
            hbox({window(text(" Vortex File Manager "), vbox(std::move(listElements))) | flex, previewBox | size(WIDTH, EQUAL, 35)}) | flex,
            text(" [Arrows]: Navigate | [Enter]: Enter | [Backspace]: Go Back | [Q]: Quit ") | center
        });
    }
};

int main()
{
    auto screen = ScreenInteractive::Fullscreen();
    VortexManager vortex;
    auto component = CatchEvent(Renderer([&] { return vortex.renderUI(); }), [&](Event event) {
        if (event == Event::Character('q') || event == Event::Character('Q') || event == Event::Escape) screen.ExitLoopClosure()();
        else if (event == Event::ArrowUp) vortex.moveUp();
        else if (event == Event::ArrowDown) vortex.moveDown();
        else if (event == Event::Return) vortex.enterDirectory();
        else if (event == Event::Backspace) vortex.leaveDirectory();
        else return false;
        return true;
    });
    screen.Loop(component);
    return 0;
}
*/
