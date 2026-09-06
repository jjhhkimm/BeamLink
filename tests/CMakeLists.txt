add_executable(beamlink_tests
  test_wav.cpp
  test_stages.cpp
  test_pipeline.cpp
)
target_link_libraries(beamlink_tests
  PRIVATE beamlink_core beamlink_warnings GTest::gtest GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(beamlink_tests)
