cmake_minimum_required(VERSION 3.10)
project(webdav_server)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加编译选项
add_compile_options(-Wall -Wextra -pthread)

# 在添加子模块之前设置全局包含路径
set(GLOBAL_INCLUDES
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/modules/auth/include
    ${PROJECT_SOURCE_DIR}/modules/http/include
    ${PROJECT_SOURCE_DIR}/modules/file/include
    ${PROJECT_SOURCE_DIR}/modules/xml/include
    ${PROJECT_SOURCE_DIR}/modules/base64/include
    ${PROJECT_SOURCE_DIR}/modules/mime/include
    ${PROJECT_SOURCE_DIR}/modules/logger/include
)

include_directories(${GLOBAL_INCLUDES})

# 添加子模块
add_subdirectory(modules/auth)
add_subdirectory(modules/http)
add_subdirectory(modules/file)
add_subdirectory(modules/xml)
add_subdirectory(modules/base64)
add_subdirectory(modules/mime)
add_subdirectory(modules/logger)

# 获取主程序源文件
file(GLOB MAIN_SOURCES "src/*.cpp")

# 生成可执行文件
add_executable(webdav_server ${MAIN_SOURCES})

# 链接所有模块库
target_link_libraries(webdav_server
    webdav_auth
    webdav_http
    webdav_file
    webdav_xml
    webdav_base64
    webdav_mime
    webdav_logger
    # pthread
) 