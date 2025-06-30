cmake_minimum_required(VERSION 3.14)
project(android_xml_converter CXX)

include(FetchContent)

option(PRETTY_PRINT "Enable pretty-print" OFF)

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(PATCH_DIR "${CMAKE_CURRENT_SOURCE_DIR}/patches")

set(ABX2XML_ORIG "${SRC_DIR}/abx2xml.cpp")
set(XML2ABX_CPP  "${SRC_DIR}/xml2abx.cpp")
set(ABX2XML_PATCHED "${CMAKE_BINARY_DIR}/abx2xml_patched.cpp")

find_package(PkgConfig)
pkg_check_modules(PUGIXML QUIET pugixml)

if(NOT PUGIXML_FOUND)
    message(STATUS "pugixml not found on, fetching from GitHub...")
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG master
    )
    FetchContent_MakeAvailable(pugixml)
    set(PUGIXML_INCLUDE_DIRS "${pugixml_SOURCE_DIR}/src")
    set(PUGIXML_LIBRARIES pugixml::pugixml)
else()
    message(STATUS "Using system-installed pugixml")
endif()

# abx2xml target
if(PRETTY_PRINT)
    # Copy and patch abx2xml.cpp to patched copy
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy "${ABX2XML_ORIG}" "${ABX2XML_PATCHED}"
    )
    execute_process(
        COMMAND patch -N "${ABX2XML_PATCHED}" "${PATCH_DIR}/pretty_print.patch"
        RESULT_VARIABLE PATCH_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(NOT PATCH_RESULT EQUAL 0 AND NOT PATCH_RESULT EQUAL 1)
        message(FATAL_ERROR "Failed to apply pretty_print.patch to abx2xml.cpp")
    endif()

    add_executable(abx2xml "${ABX2XML_PATCHED}")
    target_include_directories(abx2xml PRIVATE ${PUGIXML_INCLUDE_DIRS})
    target_link_libraries(abx2xml PRIVATE ${PUGIXML_LIBRARIES})

    # cleanup patched file after build
    add_custom_command(TARGET abx2xml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E rm -f "${ABX2XML_PATCHED}"
        COMMENT "Cleaning up patched abx2xml_patched.cpp"
    )
else()
    add_executable(abx2xml "${ABX2XML_ORIG}")
endif()

# xml2abx target
add_executable(xml2abx "${XML2ABX_CPP}")
target_include_directories(xml2abx PRIVATE ${PUGIXML_INCLUDE_DIRS})
target_link_libraries(xml2abx PRIVATE ${PUGIXML_LIBRARIES})
