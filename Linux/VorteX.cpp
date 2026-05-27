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
