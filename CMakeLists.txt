cmake_minimum_required(VERSION 3.0)
project(CuckooMap)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 11)

add_library(qdigest INTERFACE)
target_include_directories(qdigest INTERFACE 3rdParty/qdigest)

#list(APPEND CMAKE_CXX_FLAGS "-fprofile-arcs -ftest-coverage -DNDEBUG ${CMAKE_CXX_FLAGS}")
#list(APPEND CMAKE_EXE_LINKER_FLAGS "-O0 -g -fprofile-arcs -ftest-coverage -DNDEBUG ${CMAKE_EXE_LINKER_FLAGS}")


add_library(cuckoo INTERFACE)
target_include_directories(cuckoo INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(CuckooFilterTest
    tests/CuckooFilterTest.cpp
)
target_link_libraries(CuckooFilterTest PRIVATE cuckoo)

add_executable(CuckooMapTest
    tests/CuckooMapTest.cpp
)
target_link_libraries(CuckooMapTest PRIVATE cuckoo)

add_executable(CuckooMultiMapTest
    tests/CuckooMultiMapTest.cpp
)
target_link_libraries(CuckooMultiMapTest PRIVATE cuckoo)

add_executable(InternalCuckooMapTest
    tests/InternalCuckooMapTest.cpp
)
target_link_libraries(InternalCuckooMapTest PRIVATE cuckoo)

add_executable(ShardedCuckooMapTest
    tests/ShardedCuckooMapTest.cpp
)
target_link_libraries(ShardedCuckooMapTest PRIVATE cuckoo)

add_executable(ShardedCuckooMultiMapTest
    tests/ShardedCuckooMultiMapTest.cpp
)
target_link_libraries(ShardedCuckooMultiMapTest PRIVATE cuckoo)

add_executable(PerformanceTest
    tests/PerformanceTest.cpp
)
target_link_libraries(PerformanceTest PRIVATE cuckoo qdigest)

add_custom_command(TARGET PerformanceTest PRE_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_SOURCE_DIR}/performance $<TARGET_FILE_DIR:PerformanceTest>/performance)
