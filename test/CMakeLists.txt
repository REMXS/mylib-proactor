cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 23)

aux_source_directory(./ SRC)
find_package(GTest REQUIRED)

add_executable(Test)

target_sources(Test PRIVATE ${SRC})

target_link_libraries(Test PRIVATE
    GTest::gtest
    GTest::gmock
    mylib_proactor
)
target_include_directories(Test PRIVATE
    ${PROJECT_SOURCE_DIR}/include    
)