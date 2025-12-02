file(GLOB SOURCES CONFIGURE_DEPENDS
    "*.cpp" "*.hpp"
    "agents/*.cpp" "agents/*.hpp"
    "keynodes/*.hpp"
    "utils/*.cpp" "utils/*.hpp"
)

# Центр. агент пока не используется в сборке
list(FILTER SOURCES EXCLUDE REGEX "agents/find_central_district_agent\\.cpp")

add_library(transport-accessibility-module SHARED ${SOURCES})

target_link_libraries(transport-accessibility-module
    LINK_PUBLIC sc-machine::sc-memory
    LINK_PUBLIC sc-machine::sc-agents-common
    LINK_PUBLIC scl-machine::inference
    LINK_PUBLIC ps-common-lib::common-utils
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

if(${SC_BUILD_TESTS})
    add_subdirectory(test)
endif()
