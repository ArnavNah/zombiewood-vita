cmake_minimum_required(VERSION 3.14)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK to point to your SDK path!")
    endif()
endif()

project(SceLibcBridge C ASM)
include("${VITASDK}/share/vita.cmake" REQUIRED)

# Build the generated assembly with CMake. The generated Makefile uses an
# echo-based response file that quotes every object as one filename on Windows.
execute_process(COMMAND "${VITA_LIBS_GEN}"
        "${CMAKE_CURRENT_SOURCE_DIR}/nids.yml" "${CMAKE_CURRENT_BINARY_DIR}/stubs"
        RESULT_VARIABLE STUB_RESULT)
if(NOT STUB_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not generate SceLibcBridge stubs")
endif()
file(GLOB STUB_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/stubs/*.S")
add_library(SceLibcBridge STATIC ${STUB_SOURCES})
target_compile_options(SceLibcBridge PRIVATE -Wa,--defsym,GEN_WEAK_EXPORTS=0)
set_target_properties(SceLibcBridge PROPERTIES
        OUTPUT_NAME SceLibcBridge_stub
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stubs")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/stubs/
        DESTINATION lib
        FILES_MATCHING PATTERN "*.a"
)
