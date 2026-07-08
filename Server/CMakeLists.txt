cmake_minimum_required (VERSION 3.12)

project(Server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Server)

target_link_libraries(Server PUBLIC Common)
target_include_directories(Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(Server PRIVATE
	"src/ServerMain.cpp"
 "include/Diff.h" "src/Diff.cpp")