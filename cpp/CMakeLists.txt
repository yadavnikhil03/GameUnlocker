cmake_minimum_required(VERSION 3.22)
project(GameUnlockerModule)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Os -flto -ffunction-sections -fdata-sections)
add_link_options(-flto -Wl,--gc-sections)

add_library(gameunlocker SHARED
            spoof_module.cpp)

find_library(log-lib log)
target_link_libraries(gameunlocker ${log-lib})

set_target_properties(gameunlocker PROPERTIES
    LINK_FLAGS "-Wl,--exclude-libs,ALL"
)
