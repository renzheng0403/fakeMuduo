include_directories("${PROJECT_SOURCE_DIR}/base")

add_executable(atomic_unittest Atomic_unittest.cc)
add_test(NAME atomic_unittest COMMAND atomic_unittest)

add_executable(timestamp_unittest Timestamp_unittest.cc)
target_link_libraries(timestamp_unittest fakeMuduo_base)
add_test(NAME timestamp_unittest COMMAND timestamp_unittest)

add_executable(thread_test Thread_test.cc)
target_link_libraries(thread_test fakeMuduo_base)