// beamlink in.wav out.wav
//
// Phase 1: every stage between the WAV source and the WAV sink is a
// passthrough, so out.wav is byte-for-byte identical to in.wav. That identity
// is the regression anchor for the phases that follow.

#include <cstdio>
#include <exception>
#include <string>

#include "beamlink/pipeline.h"
#include "beamlink/stages.h"

namespace {

int Usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s <in.wav> <out.wav>\n"
               "\n"
               "Runs 16-bit mono PCM through the Source -> Encoder ->\n"
               "Transport -> Decoder -> Sink pipeline in %d-sample frames.\n",
               argv0, beamlink::kFrameSamples);
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    return Usage(argv[0]);
  }
  const std::string in_path = argv[1];
  const std::string out_path = argv[2];

  try {
    beamlink::WavFileSource source(in_path);
    beamlink::PassthroughEncoder encoder;
    beamlink::LoopbackTransport transport;
    beamlink::PassthroughDecoder decoder;
    beamlink::WavFileSink sink(out_path, source.format(),
                               source.total_samples());

    beamlink::Pipeline pipeline;
    pipeline.source = &source;
    pipeline.encoder = &encoder;
    pipeline.transport = &transport;
    pipeline.decoder = &decoder;
    pipeline.sink = &sink;

    const beamlink::PipelineStats stats = pipeline.run();

    std::printf("%s -> %s\n", in_path.c_str(), out_path.c_str());
    std::printf("  %u Hz mono 16-bit, %llu samples\n", source.format().sample_rate,
                static_cast<unsigned long long>(source.total_samples()));
    std::printf("  %llu frames of %d samples (%.1f ms)\n",
                static_cast<unsigned long long>(stats.frames_read),
                beamlink::kFrameSamples, beamlink::kFrameDurationUs / 1000.0);
    std::printf("  %llu packets sent, %llu received\n",
                static_cast<unsigned long long>(stats.packets_sent),
                static_cast<unsigned long long>(stats.packets_received));
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "beamlink: %s\n", e.what());
    return 1;
  }
}
