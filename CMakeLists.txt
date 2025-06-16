cmake_minimum_required(VERSION 3.13)

project(android_xml_converter LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(abx2xml abx2xml.cpp)
add_executable(xml2abx xml2abx.cpp)
