cmake_minimum_required(VERSION 3.10)
project(webdav_server)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Android NDK 特定设置
if(ANDROID)
    # 使用 Android 的线程库
    find_library(ANDROID_THREAD_LIB c)
    set(THREAD_LIB ${ANDROID_THREAD_LIB})
else()
    # 使用标准 pthread
    set(THREAD_LIB pthread)
endif()

# 添加编译选项
add_compile_options(-Wall -Wextra)

# 添加子目录
add_subdirectory(modules/logger)
add_subdirectory(modules/auth)
add_subdirectory(modules/http)
add_subdirectory(modules/file)
add_subdirectory(modules/xml)
add_subdirectory(modules/base64)
add_subdirectory(modules/mime)

# 包含头文件目录
include_directories(
    include
    modules/auth/include
    modules/http/include
    modules/file/include
    modules/xml/include
    modules/base64/include
    modules/mime/include
    modules/logger/include
)

# 添加源文件
add_executable(webdav_server
    src/main.cpp
    src/webdav_server.cpp
    src/webdav_handlers.cpp
)

# 链接库
target_link_libraries(webdav_server
    webdav_auth
    webdav_http
    webdav_file
    webdav_xml
    webdav_base64
    webdav_mime
    webdav_logger
    ${THREAD_LIB}
)

# 如果是 Android，添加特定的链接选项
if(ANDROID)
    target_link_libraries(webdav_server log)  # 添加 Android 日志库
endif() 