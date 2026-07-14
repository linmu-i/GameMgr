cmake_minimum_required (VERSION 3.12)

project(Client)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Client)

target_compile_options(Client PRIVATE 
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-finput-charset=UTF-8 -fexec-charset=UTF-8>
)

target_link_libraries(Client PUBLIC Common)
target_link_libraries(Client PUBLIC EbbGlow)
target_include_directories(Client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_sources(Client PRIVATE
    "src/ClientMain.cpp"
    "src/Sync.cpp"
    "src/Config.cpp"
    "src/Global.cpp"
    "src/GUI.cpp"
    "src/Win32.cpp"
)