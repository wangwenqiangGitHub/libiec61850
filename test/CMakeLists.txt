set(test_map
   ${CMAKE_CURRENT_SOURCE_DIR}/map_test.c
   ${CMAKE_CURRENT_SOURCE_DIR}/../hal/memory/lib_memory.c
   ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/map.c
   ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/linked_list.c
   ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/string_map.c
   )
   message(${test_map})
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}/config
   ${CMAKE_CURRENT_SOURCE_DIR}/../src/common/inc
    ${CMAKE_CURRENT_LIST_DIR}/src/common/inc
   ${CMAKE_CURRENT_SOURCE_DIR}/../hal/inc
)
add_executable(test_map ${test_map})
