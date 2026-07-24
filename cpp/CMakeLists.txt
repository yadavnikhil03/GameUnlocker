cmake_minimum_required(VERSION 3.22)
project(GameUnlockerModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-flto -Wl,--gc-sections)

add_subdirectory(bytehook)
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
            Companion.cpp
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

add_executable(gu_controller controller.cpp)
target_link_libraries(gu_controller ${log-lib})
set_target_properties(gu_controller PROPERTIES
    LINK_FLAGS "-Wl,--gc-sections"
)
