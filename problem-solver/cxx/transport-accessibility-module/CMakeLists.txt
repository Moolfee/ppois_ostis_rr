file(GLOB SOURCES CONFIGURE_DEPENDS
    "*.cpp" "*.hpp"
    "agents/*.cpp" "agents/*.hpp"
    "keynodes/*.hpp"
    "utils/*.cpp" "utils/*.hpp"
)

add_library(transport-accessibility-module SHARED ${SOURCES})

target_link_libraries(transport-accessibility-module
    LINK_PUBLIC sc-machine::sc-memory
    LINK_PUBLIC sc-machine::sc-agents-common
    LINK_PUBLIC scl-machine::inference
    LINK_PUBLIC ps-common-lib::common-utils
)

target_link_libraries(transport-accessibility-module
    LINK_PUBLIC sc-machine::sc-memory
)

target_include_directories(transport-accessibility-module
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

if(NOT DEFINED SC_EXTENSIONS_DIRECTORY)
    # Default output dir for compiled extensions when the variable is not provided by sc-machine
    set(SC_EXTENSIONS_DIRECTORY "${CMAKE_BINARY_DIR}/extensions")
endif()

set_target_properties(transport-accessibility-module
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${SC_EXTENSIONS_DIRECTORY}
)

# Тесты для модуля транспортной доступности
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
    "test/*.cpp"
)

if(SC_BUILD_TESTS)
    add_executable(transport-accessibility-module-tests ${TEST_SOURCES})

    target_link_libraries(transport-accessibility-module-tests
        LINK_PRIVATE GTest::gtest_main
        LINK_PRIVATE transport-accessibility-module
    )

    target_include_directories(transport-accessibility-module-tests
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    )

    gtest_discover_tests(transport-accessibility-module-tests
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    )
endif()
