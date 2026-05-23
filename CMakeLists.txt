cmake_minimum_required(VERSION 3.14...3.20)
project(sqb VERSION 1.0.0 LANGUAGES C CXX)

SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_EXAMPLES "Build example programs" OFF)
option(USE_INTERNAL_SQUIRREL "Download and build squirrel automatically" ON)


if(USE_INTERNAL_SQUIRREL)
    message(STATUS "Using internal squirrel (will download from GitHub)")

    include(FetchContent)
    FetchContent_Declare(
        squirrel
        GIT_REPOSITORY https://github.com/Ligverd/squirrel.git
        #GIT_TAG master
        GIT_TAG b29d442bfa74ff270c5749007c068c28485e8708
        GIT_SHALLOW TRUE
    )

    set(SQ_DISABLE_INTERPRETER ON CACHE BOOL "" FORCE)
    set(DISABLE_STATIC OFF CACHE BOOL "" FORCE)
    set(DISABLE_DYNAMIC ON CACHE BOOL "" FORCE)
    set(SQ_ENABLE_ALL_LIBS ON CACHE BOOL "" FORCE)
    set(SQ_STDLIB ON CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(squirrel)
    set(SQUIRREL_LIBRARIES squirrel_static sqstdlib_static)
else()
    message(STATUS "Using external squirrel")
    find_package(squirrel REQUIRED)
    set(SQUIRREL_LIBRARIES squirrel::squirrel_static squirrel::sqstdlib_static)
endif()


file(GLOB CppFiles src/*.cpp)
file(GLOB HeaderFiles src/*.h)


add_library(${PROJECT_NAME} ${CppFiles} ${HeaderFiles})


target_compile_definitions(${PROJECT_NAME} PUBLIC SQBINDING_EXISTS)


target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)


target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)


target_link_libraries(${PROJECT_NAME} PUBLIC ${SQUIRREL_LIBRARIES})


if(USE_INTERNAL_SQUIRREL AND DEFINED squirrel_SOURCE_DIR)
    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
        ${squirrel_SOURCE_DIR}/include
    )
endif()


if(BUILD_EXAMPLES AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples)
    add_subdirectory(examples)
endif()
