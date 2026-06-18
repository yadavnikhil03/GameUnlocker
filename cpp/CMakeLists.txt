cmake_minimum_required(VERSION 3.22)
project(GameUnlockerModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-flto -Wl,--gc-sections)

# bytehook is vendored under cpp/bytehook (v1.0.9, the last release that does
# not depend on shadowhook). Building it via add_subdirectory keeps the build
# fully offline and avoids the v1.1.0+ CMake 3.30 / shadowhook requirements.
add_subdirectory(bytehook)

# bytehook's own CMake enables -Weverything -Werror on its sources; we do not
# want its warnings to affect our build, so disable them on the bytehook target.
if(TARGET bytehook)
    target_compile_options(bytehook PRIVATE
        -Wno-everything
        -Wno-declaration-after-statement)
    set_target_properties(bytehook PROPERTIES
        INTERFACE_COMPILE_OPTIONS ""
        INTERFACE_LINK_OPTIONS "")
endif()

add_library(gameunlocker SHARED
            main.cpp
            Config.cpp
            Spoofer.cpp
            GpuHook.cpp
            SysPropHook.cpp
            HookManager.cpp
            RoutingEngine.cpp)

find_library(log-lib log)
target_link_libraries(gameunlocker ${log-lib} bytehook)

target_include_directories(gameunlocker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/bytehook/include)

set_target_properties(gameunlocker PROPERTIES
    LINK_FLAGS "-Wl,--exclude-libs,ALL"
)
