cmake_minimum_required(VERSION 3.20)
project(LiveTextSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages
find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

# freetype
pkg_check_modules(FREETYPE REQUIRED freetype2)

# For testing without Aeron, we'll create stub implementations
add_definitions(-DTESTING_MODE)

# imgui - Download if not present
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
if(NOT EXISTS ${IMGUI_DIR})
    message(STATUS "Downloading ImGui...")
    execute_process(
        COMMAND git clone --depth 1 --branch v1.90 https://github.com/ocornut/imgui.git ${IMGUI_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    )
endif()

file(GLOB IMGUI_SOURCES
    ${IMGUI_DIR}/*.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# gl3w - simple OpenGL loader
set(GL3W_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/gl3w)
if(NOT EXISTS ${GL3W_DIR})
    message(STATUS "Setting up gl3w...")
    file(MAKE_DIRECTORY ${GL3W_DIR}/include/GL)
    file(MAKE_DIRECTORY ${GL3W_DIR}/src)

    # Download gl3w files
    file(DOWNLOAD
        "https://raw.githubusercontent.com/skaslev/gl3w/master/include/GL/gl3w.h"
        "${GL3W_DIR}/include/GL/gl3w.h"
    )
    file(DOWNLOAD
        "https://raw.githubusercontent.com/skaslev/gl3w/master/include/GL/glcorearb.h"
        "${GL3W_DIR}/include/GL/glcorearb.h"
    )
    file(DOWNLOAD
        "https://raw.githubusercontent.com/skaslev/gl3w/master/src/gl3w.c"
        "${GL3W_DIR}/src/gl3w.c"
    )
endif()

# Create network layer with real UDP implementation
add_library(aeron_stub
    src/testing/AeronStub.cpp
    src/network/UdpNetworkLayer.cpp
    src/network/NetworkAdapter.cpp
)

target_include_directories(aeron_stub PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Common library (with stubs for testing)
add_library(common
    src/common/TextMessage.cpp
    src/common/HealthMonitor.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(common aeron_stub)

# Sender application
add_executable(sender
    src/sender/main.cpp
    src/sender/SenderApp.cpp
    src/sender/TextMemory.cpp
    src/sender/NDIReceiver.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(sender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${GL3W_DIR}/include
)

target_link_libraries(sender
    common
    glfw
    ${OPENGL_LIBRARIES}
)

# Receiver application
add_executable(receiver
    src/receiver/main.cpp
    src/receiver/ReceiverApp.cpp
    src/receiver/TextRenderer.cpp
    src/receiver/TextureSender.cpp
    src/receiver/SyphonSender.mm
    ${GL3W_DIR}/src/gl3w.c
    ${IMGUI_SOURCES}
)

target_include_directories(receiver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FREETYPE_INCLUDE_DIRS}
    ${GL3W_DIR}/include
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_compile_options(receiver PRIVATE ${FREETYPE_CFLAGS_OTHER})

target_link_libraries(receiver
    common
    glfw
    ${OPENGL_LIBRARIES}
)

# Link FreeType using pkg-config results
target_link_libraries(receiver ${FREETYPE_LIBRARIES})
target_link_directories(receiver PRIVATE ${FREETYPE_LIBRARY_DIRS})

# NDI SDK configuration (optional)
option(ENABLE_NDI "Enable NDI support" ON)

if(ENABLE_NDI)
    # Try to find NDI SDK
    find_path(NDI_INCLUDE_DIR Processing.NDI.Lib.h
        PATHS
        "/Library/NDI SDK for Apple/include"
        "$ENV{HOME}/Library/NDI SDK for Apple/include"
        "/usr/local/include"
    )

    find_library(NDI_LIBRARY ndi
        PATHS
        "/Library/NDI SDK for Apple/lib/macOS"
        "$ENV{HOME}/Library/NDI SDK for Apple/lib/macOS"
        "/usr/local/lib"
    )

    if(NDI_INCLUDE_DIR AND NDI_LIBRARY)
        message(STATUS "Found NDI SDK:")
        message(STATUS "  Include: ${NDI_INCLUDE_DIR}")
        message(STATUS "  Library: ${NDI_LIBRARY}")

        target_include_directories(sender PRIVATE ${NDI_INCLUDE_DIR})
        target_link_libraries(sender ${NDI_LIBRARY})
        target_compile_definitions(sender PRIVATE ENABLE_NDI)
    else()
        message(WARNING "NDI SDK not found. NDI support will be disabled.")
        message(STATUS "  To enable NDI, install the NDI SDK and ensure it's in the search paths.")
    endif()
endif()

# Platform-specific libraries and frameworks
if(APPLE)
    # Silence OpenGL deprecation warnings on macOS
    target_compile_definitions(sender PRIVATE GL_SILENCE_DEPRECATION)
    target_compile_definitions(receiver PRIVATE GL_SILENCE_DEPRECATION)
    # Find and link Syphon framework
    # First try user's Frameworks directory, then system Frameworks
    find_library(SYPHON_FRAMEWORK Syphon PATHS ~/Library/Frameworks /Library/Frameworks)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)

    if(SYPHON_FRAMEWORK)
        target_link_libraries(receiver ${SYPHON_FRAMEWORK} ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK})
        target_compile_definitions(receiver PRIVATE __APPLE__)
        message(STATUS "Found Syphon framework: ${SYPHON_FRAMEWORK}")
    else()
        message(WARNING "Syphon framework not found - Syphon support disabled")
        target_link_libraries(receiver ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK})
        target_compile_definitions(receiver PRIVATE __APPLE__)
    endif()
endif()

# Copy fonts directory to build directory
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/fonts)
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fonts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()