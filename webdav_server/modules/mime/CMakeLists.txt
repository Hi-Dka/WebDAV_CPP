add_library(webdav_mime STATIC
    src/mime_types.cpp
)

target_include_directories(webdav_mime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 