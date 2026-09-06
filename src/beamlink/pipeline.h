#pragma once

// Drives the five stages. The pipeline itself holds no audio knowledge beyond
// the frame unit, so replacing any stage in a later phase needs no change here.

#include "beamlink/stage.h"

namespace beamlink {

struct PipelineStats {
  uint64_t frames_read = 0;
  uint64_t packets_sent = 0;
  uint64_t packets_received = 0;
  uint64_t frames_written = 0;
};

// Non-owning view of the stages; the caller keeps them alive for the run.
struct Pipeline {
  Source* source = nullptr;
  Encoder* encoder = nullptr;
  Transport* transport = nullptr;
  Decoder* decoder = nullptr;
  Sink* sink = nullptr;

  // Pumps every frame from source to sink and finishes the sink. Throws
  // std::invalid_argument if any stage is null.
  PipelineStats run();
};

}  // namespace beamlink
