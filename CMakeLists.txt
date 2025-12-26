cmake_minimum_required(VERSION 3.20)
project(zk_pbr_renderer)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加include目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 添加库目录
link_directories(${CMAKE_SOURCE_DIR}/lib)

# 显式列出源文件（比GLOB更可靠）
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/main.cpp
    ${CMAKE_SOURCE_DIR}/src/glad.c
)

# 创建可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    glfw3
    opengl32  # Windows OpenGL库
)

# Windows特定设置
if(WIN32)
    # 设置为控制台程序（可以看到std::cout输出）
    # 如果不想要控制台，改为 WIN32
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE OFF
    )
endif()