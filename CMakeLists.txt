cmake_minimum_required(VERSION 3.22)

project(Maze)

add_executable(Maze main.c game.c)
target_link_libraries(Maze PRIVATE pthread)

