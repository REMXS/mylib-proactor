cmake_minimum_required(VERSION 3.15.0)

set(CMAKE_CXX_STANDARD 23)

find_package(GTest REQUIRED)

#unit test
aux_source_directory(./unit_test UNIT_TEST_SRC)
add_executable(UnitTest)
target_sources(UnitTest PRIVATE ${UNIT_TEST_SRC})
target_link_libraries(UnitTest PRIVATE
    GTest::gtest
    GTest::gmock
    mylib_proactor
)
target_include_directories(UnitTest PRIVATE
    ${PROJECT_SOURCE_DIR}/include    
)


#integration test
aux_source_directory(./integration_test INTEGRATION_TEST_SRC)
add_executable(IntegrationTest)
target_sources(IntegrationTest PRIVATE ${INTEGRATION_TEST_SRC})
target_link_libraries(IntegrationTest PRIVATE
    GTest::gtest
    GTest::gmock
    mylib_proactor
)
target_include_directories(IntegrationTest PRIVATE
    ${PROJECT_SOURCE_DIR}/include    
)