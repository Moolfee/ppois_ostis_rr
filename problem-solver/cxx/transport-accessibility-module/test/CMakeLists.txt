file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

add_executable(transport-accessibility-module-tests ${TEST_SOURCES})

target_link_libraries(transport-accessibility-module-tests
    LINK_PRIVATE transport-accessibility-module
    LINK_PRIVATE GTest::gtest_main
)

target_include_directories(transport-accessibility-module-tests
    PRIVATE ${CMAKE_CURRENT_LIST_DIR}/..
)

gtest_discover_tests(transport-accessibility-module-tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
