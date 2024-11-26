add_library(webdav_logger STATIC
    src/logger.cpp
)

target_include_directories(webdav_logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 