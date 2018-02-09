cmake_minimum_required(VERSION 3.9)
project(gift-exchange)

set(CMAKE_CXX_STANDARD 14)

find_package(Boost 1.60 REQUIRED
        program_options
        )

add_executable(gift-exchange
        main.cpp
        )
target_include_directories(gift-exchange SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_link_libraries(gift-exchange ${Boost_LIBRARIES})
