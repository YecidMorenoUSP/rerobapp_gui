cmake_minimum_required(VERSION 3.10)
project(master_gui)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)

# Add source files
set(IMGUI_DIR "/home/yecid/Desktop/docker/rerobapp_gui/src/imgui")
set(SOURCE_FILES
    master_gui.cpp
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)

# Find packages
find_package(SDL2 REQUIRED)
find_package(Threads REQUIRED)

# Platform-specific configurations
if(UNIX AND NOT APPLE)
    set(ECHO_MESSAGE "Linux")
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    include_directories("/home/yecid/Desktop/docker/rerobapp_gui/src/json/include")
    include_directories("/home/yecid/Desktop/docker/rerobapp_gui/src/libs/")

    # Consolidate all library links
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        ${SDL2_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT}
        GL
        dl
    )

    # Compiler options
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pthread)
endif()

# Build messages
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Build complete for ${ECHO_MESSAGE}"
)
