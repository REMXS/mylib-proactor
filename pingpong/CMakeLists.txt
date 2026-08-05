cmake_minimum_required(VERSION 3.15.0)
project(pingpong LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 默认 Release：压测必须开启优化。
# 注意：不要复用根目录的 build（那里编译库时开了 -fsanitize=address，
# 会严重拖慢压测性能，测出来的数据没有参考价值）。
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)
find_package(Threads REQUIRED)

# mylib-proactor 库源码直接编译进本程序，避免链接根目录那个带 ASan 的库
file(GLOB MYLIB_SRC ${PROJECT_SOURCE_DIR}/../src/*.cc)

add_executable(pingpong_server server.cc ${MYLIB_SRC})
target_include_directories(pingpong_server PRIVATE
    ${LIBURING_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/../include
)
target_link_directories(pingpong_server PRIVATE ${LIBURING_LIBRARY_DIRS})
target_link_libraries(pingpong_server PRIVATE ${LIBURING_LIBRARIES} Threads::Threads)

# 客户端不需要库，纯阻塞 socket + poll
add_executable(pingpong_client client.cc)
target_link_libraries(pingpong_client PRIVATE Threads::Threads)
