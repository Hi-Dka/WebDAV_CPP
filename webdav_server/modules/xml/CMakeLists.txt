add_library(webdav_xml STATIC
    src/xml_parser.cpp
)

target_include_directories(webdav_xml PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
) 