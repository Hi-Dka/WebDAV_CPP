# WebDAV Server

一个基于C++11的轻量级WebDAV服务器实现，支持标准WebDAV协议，可以在Linux和Android平台上运行。

## 功能特点

- 完整支持WebDAV协议
- 基于模块化设计
- 跨平台支持(Linux/Android)
- 身份验证支持
- 文件操作管理
- 日志系统

## 项目结构

    webdav_server/
    ├── modules/
    │   ├── auth/      # 身份验证模块
    │   ├── http/      # HTTP/WebDAV协议处理
    │   ├── file/      # 文件系统操作
    │   ├── xml/       # XML解析
    │   ├── base64/    # Base64编解码
    │   ├── mime/      # MIME类型处理
    │   └── logger/    # 日志系统
    └── src/           # 主程序源码

## 构建要求

- CMake 3.10+
- C++11兼容的编译器
- pthread支持
- Android NDK (可选，用于Android平台构建)

## 构建步骤

    mkdir build
    cd build
    cmake ..
    make

## Android构建

使用Android NDK进行交叉编译：

    mkdir build-android
    cd build-android
    cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_PATH/build/cmake/android.toolchain.cmake ..
    make

## 使用方法

运行编译好的服务器程序：

    ./webdav_server [配置文件路径]

## 许可证

MIT License

## 贡献指南

欢迎提交Issue和Pull Request。

## 作者

hidka