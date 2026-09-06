add_executable(beamlink_bench bench_pipeline.cpp)
target_link_libraries(beamlink_bench
  PRIVATE beamlink_core beamlink_warnings benchmark::benchmark)
