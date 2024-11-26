add_library(webdav_auth STATIC
    src/auth_manager.cpp
)

target_include_directories(webdav_auth PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 