add_library(webdav_http STATIC
    src/http_parser.cpp
)

target_include_directories(webdav_http PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 