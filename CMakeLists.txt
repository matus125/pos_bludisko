cmake_minimum_required(VERSION 3.22)
project(MazeGame)

add_executable(server server.c ipc.c)
add_executable(client client.c ipc.c)

target_link_libraries(server pthread rt)
target_link_libraries(client pthread rt)