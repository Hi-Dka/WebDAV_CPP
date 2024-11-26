add_library(webdav_base64 STATIC
    src/base64.cpp
)

target_include_directories(webdav_base64 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 