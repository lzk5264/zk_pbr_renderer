cmake_minimum_required(VERSION 3.20)
project(zk_pbr_renderer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows平台设置UTF-8编码
if(MSVC)
    add_compile_options(/utf-8)
endif()

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)

# 添加库目录
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 显式列出源文件（比GLOB更可靠）
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/core/window.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/shader.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/mesh.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/vertex_layout.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/texture.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/texture2d.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/texture_cubemap.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/camera.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/camera_controller.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/geometry_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/uniform_buffer.cpp
    ${CMAKE_SOURCE_DIR}/src/gfx/framebuffer.cpp
    ${CMAKE_SOURCE_DIR}/src/glad.c
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    glfw3
    opengl32  # Windows OpenGL库
)

# 复制shaders目录到构建输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMENT "Copying shaders to build directory..."
)

# 复制resources目录到构建输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
    COMMENT "Copying resources to build directory..."
)

# Windows特定设置
if(WIN32)
    # 设置为控制台程序（可以看到std::cout输出）
    # 如果不想要控制台，改为 WIN32
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE OFF
    )
endif()