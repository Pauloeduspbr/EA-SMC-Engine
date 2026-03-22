cmake_minimum_required(VERSION 3.10)
project(smc_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Sources
set(SOURCES
    src/swing_detector.cpp
    src/structure_analyzer.cpp
    src/ob_tracker.cpp
    src/fvg_detector.cpp
    src/liquidity_mapper.cpp
    src/signal_generator.cpp
    src/smc_engine.cpp
)

# Include directories
include_directories(include)

# Build shared library (DLL on Windows, .so on Linux)
add_library(smc_engine SHARED ${SOURCES})

# Windows-specific: remove "lib" prefix, static link MinGW runtime
if(WIN32 OR MINGW)
    set_target_properties(smc_engine PROPERTIES PREFIX "")
    target_compile_definitions(smc_engine PRIVATE _WIN32)
    # Static link libgcc, libstdc++, libwinpthread — eliminates DLL dependencies
    target_link_options(smc_engine PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(smc_engine PRIVATE -O2)
endif()
