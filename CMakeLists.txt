cmake_minimum_required(VERSION 3.16)
project(usm_toolkit VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform detection
if(WIN32)
    add_definitions(-DPLATFORM_WINDOWS)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DNOMINMAX)
elseif(UNIX AND NOT APPLE)
    add_definitions(-DPLATFORM_LINUX)
elseif(APPLE)
    add_definitions(-DPLATFORM_MACOS)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/byte_utils.cpp
    src/file_utils.cpp
    src/mpeg_stream.cpp
    src/usm_stream.cpp
)

# Header files
set(HEADERS
    include/byte_utils.hpp
    include/file_utils.hpp
    include/mpeg_stream.hpp
    include/usm_stream.hpp
    include/usm_toolkit.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

# Compiler warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Link filesystem library on older GCC
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
    target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
endif()

# Install rules
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(FILES config/config.json DESTINATION bin)

# Copy config to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config/config.json 
               ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)

message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
