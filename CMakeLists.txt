cmake_minimum_required(VERSION 3.20)

project(dev_challenge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FRAMEWORK_SOURCES
        src/antworld.cpp
        src/useable_functions.cpp
)

set(SOLUTION "applicant" CACHE STRING "Solution to compile: applicant, v1, v2, v3_1, v3_2, or v4")
set_property(CACHE SOLUTION PROPERTY STRINGS applicant v1 v2 v3_1 v3_2 v4)
if (SOLUTION STREQUAL "applicant")
    set(SOLUTION_SOURCE src/applicant_solution.cpp)
elseif (SOLUTION MATCHES "^(v1|v2|v3_1|v3_2|v4)$")
    set(SOLUTION_SOURCE solutions/${SOLUTION}_solution.cpp)
else ()
    message(FATAL_ERROR "Unknown SOLUTION '${SOLUTION}'. Choose applicant, v1, v2, v3_1, v3_2, or v4.")
endif ()

add_executable(dev_challenge
        src/main.cpp
        ${SOLUTION_SOURCE}
        ${FRAMEWORK_SOURCES}
)

target_include_directories(dev_challenge PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(dev_challenge PRIVATE /W4)
else ()
    target_compile_options(dev_challenge PRIVATE -Wall -Wextra -Wpedantic)
endif ()

include(CTest)

if (BUILD_TESTING)
    add_executable(antworld_tests
            test/antworld_tests.cpp
            src/applicant_solution.cpp
            ${FRAMEWORK_SOURCES}
    )

    target_include_directories(antworld_tests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    if (MSVC)
        target_compile_options(antworld_tests PRIVATE /W4)
    else ()
        target_compile_options(antworld_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif ()

    add_test(
            NAME antworld_framework_tests
            COMMAND antworld_tests
    )
endif ()
