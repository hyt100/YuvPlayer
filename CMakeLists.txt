cmake_minimum_required(VERSION 3.0)
project(YuvPlayer)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INSTALL_PREFIX ${PROJECT_BINARY_DIR}/install/)

add_executable(${PROJECT_NAME} src/player.cc)
target_link_libraries(${PROJECT_NAME} SDL2)