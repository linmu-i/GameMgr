cmake_minimum_required (VERSION 3.12)

project(Common)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(Common)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/TideEcho/TideEcho")

target_link_libraries(Common PUBLIC TideEcho)
target_include_directories(Common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/extern/EbbGlow/EbbGlow/include")
target_include_directories(Common PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(Common PRIVATE
"src/DataType.cpp"
"src/Type.cpp")