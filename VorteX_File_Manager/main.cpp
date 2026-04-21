#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

class VortexExplorer
{
public:
    VortexExplorer(std::string start_path) : current_path(start_path)
    {
    }

    void ReadDirectory()
    {
        directory_content.clear();

        try
        {
            if (fs::exists(current_path) && fs::is_directory(current_path))
            {
                for (const auto& entry : fs::directory_iterator(current_path))
                {
                    directory_content.push_back(entry);
                }
                
                std::sort(directory_content.begin(), directory_content.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) 
                {
                    if (a.is_directory() != b.is_directory())
                    {
                        return a.is_directory() > b.is_directory();
                    }
                    return a.path().filename() < b.path().filename();
                });
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Critical Error: " << e.what() << std::endl;
        }
    }

    void Render()
    {
        std::cout << "VorteX Explorer | Current Path: " << current_path << std::endl;
        std::cout << "==========================================================" << std::endl;

        for (const auto& item : directory_content)
        {
            std::string prefix = item.is_directory() ? "[DIR]  " : "[FILE] ";
            std::cout << prefix << item.path().filename().string() << std::endl;
        }
    }

private:
    std::string current_path;
    std::vector<fs::directory_entry> directory_content;
};

int main()
{
    VortexExplorer vortex(".");
    vortex.ReadDirectory();
    vortex.Render();

    return 0;
}
