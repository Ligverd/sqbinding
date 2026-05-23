cmake_minimum_required(VERSION 3.14)
project(sqb_examples LANGUAGES C CXX)

add_executable(test test.cpp ut/check.h)
target_link_libraries(test PRIVATE sqb)

add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE sqb)

if(CMAKE_INSTALL_PREFIX)
    install(TARGETS test demo
        RUNTIME DESTINATION bin/examples
    )
endif()
