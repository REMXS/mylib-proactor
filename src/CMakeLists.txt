cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 23)

aux_source_directory(./ SRC)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBURING REQUIRED liburing)

add_library(mylib_proactor SHARED)


target_include_directories(mylib_proactor PRIVATE 
    ${LIBURING_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
)

target_link_directories(mylib_proactor PRIVATE ${LIBURING_LIBRARY_DIRS})
target_link_libraries(mylib_proactor PRIVATE ${LIBURING_LIBRARIES})
target_sources(mylib_proactor PRIVATE ${SRC})
