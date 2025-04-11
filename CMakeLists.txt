cmake_minimum_required(VERSION 3.31)

project(team_picker)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

add_executable(team_picker "main.cpp")

target_sources(team_picker PUBLIC "main.cpp")
target_sources(team_picker PRIVATE "team_data.txt" "composition.txt" "readme.md")