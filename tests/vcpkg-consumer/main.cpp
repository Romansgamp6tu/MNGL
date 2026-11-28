#define APP_NAME "MNGL vcpkg consumer smoke"
#define APP_FLAGS 0

#include <MNGL/paths.hpp>

#include <filesystem>
#include <iostream>

int main()
{
    try
    {
        mngl::runtime::ensure();
        const auto directory = mngl::paths::shader_directory("batcher2d.vert.hlsl");
        if (!std::filesystem::is_regular_file(directory / "batcher2d.vert.hlsl"))
        {
            std::cerr << "MNGL could not locate its installed runtime shaders.\n";
            return 1;
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
