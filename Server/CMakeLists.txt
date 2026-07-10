cmake_minimum_required (VERSION 3.12)

project(Server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Server)

target_compile_options(Server PRIVATE 
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-finput-charset=UTF-8 -fexec-charset=UTF-8>
)

target_link_libraries(Server PUBLIC Common)
target_include_directories(Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(Server PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../Common/extern/EbbGlow/EbbGlow/extern")

target_sources(Server PRIVATE
	"src/ServerMain.cpp"
	"src/Diff.cpp"
	"src/Log.cpp")