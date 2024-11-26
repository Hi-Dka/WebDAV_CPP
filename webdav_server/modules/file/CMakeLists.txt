add_library(webdav_file STATIC
    src/file_manager.cpp
)

target_include_directories(webdav_file PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/modules/logger/include
) 