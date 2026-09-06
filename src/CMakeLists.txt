add_library(beamlink_core
  beamlink/wav.cpp
  beamlink/stages.cpp
  beamlink/pipeline.cpp
)
target_include_directories(beamlink_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(beamlink_core PRIVATE beamlink_warnings)

add_executable(beamlink main.cpp)
target_link_libraries(beamlink PRIVATE beamlink_core beamlink_warnings)
