cmake_minimum_required(VERSION 3.22)
project(GameUnlockerModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-flto -Wl,--gc-sections)

include(FetchContent)
FetchContent_Declare(
  bhook
  GIT_REPOSITORY https://github.com/bytedance/bhook.git
  GIT_TAG        v1.0.8
  SOURCE_SUBDIR  bytehook/src/main/cpp
)
FetchContent_MakeAvailable(bhook)

add_library(gameunlocker SHARED
            main.cpp
            Config.cpp
            Companion.cpp
            Spoofer.cpp
            GpuHook.cpp
            SysPropHook.cpp
            HookManager.cpp
            RoutingEngine.cpp)

find_library(log-lib log)
target_link_libraries(gameunlocker ${log-lib} bytehook)

target_include_directories(gameunlocker PRIVATE include)

set_target_properties(gameunlocker PROPERTIES
    LINK_FLAGS "-Wl,--exclude-libs,ALL"
)
