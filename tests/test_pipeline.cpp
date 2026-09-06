// The Phase 1 acceptance test: the whole Source -> Encoder -> Transport ->
// Decoder -> Sink path must reproduce its input byte for byte. Every later
// phase keeps this file green by construction or explains why it cannot.

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "beamlink/pipeline.h"
#include "beamlink/stages.h"
#include "test_support.h"

namespace {

using namespace beamlink;
using namespace beamlink_test;

// Runs the standard Phase 1 stage set over `in_path`, writing `out_path`.
PipelineStats RunPassthrough(const std::string& in_path,
                             const std::string& out_path) {
  WavFileSource source(in_path);
  PassthroughEncoder encoder;
  LoopbackTransport transport;
  PassthroughDecoder decoder;
  WavFileSink sink(out_path, source.format(), source.total_samples());

  Pipeline pipeline;
  pipeline.source = &source;
  pipeline.encoder = &encoder;
  pipeline.transport = &transport;
  pipeline.decoder = &decoder;
  pipeline.sink = &sink;
  return pipeline.run();
}

class PassthroughPipeline : public testing::TestWithParam<size_t> {};

TEST_P(PassthroughPipeline, OutputIsBitIdenticalToInput) {
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string out = dir.file("out.wav");

  const std::vector<int16_t> samples = MakeSweep(GetParam());
  const std::vector<uint8_t> original = MakeWavBytes(samples, 16000);
  WriteBytes(in, original);

  const PipelineStats stats = RunPassthrough(in, out);

  const size_t expected_frames =
      (GetParam() + kFrameSamples - 1) / kFrameSamples;
  EXPECT_EQ(stats.frames_read, expected_frames);
  EXPECT_EQ(stats.packets_sent, expected_frames);
  EXPECT_EQ(stats.packets_received, expected_frames);
  EXPECT_EQ(stats.frames_written, expected_frames);

  EXPECT_EQ(ReadBytes(out), original);
}

// Lengths chosen around the frame boundary: empty, sub-frame, exact multiple,
// one over, one under, and a long stream.
INSTANTIATE_TEST_SUITE_P(Lengths, PassthroughPipeline,
                         testing::Values(0, 1, 63, 64, 65, 127, 128, 16000));

TEST(PassthroughPipeline, PreservesFullScaleSamples) {
  // A sweep never reaches the rails, so clipping or sign errors at the extremes
  // would slip past the parameterized test above.
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string out = dir.file("out.wav");

  std::vector<int16_t> samples(kFrameSamples * 2);
  for (size_t i = 0; i < samples.size(); ++i) {
    samples[i] = (i % 2 == 0) ? -32768 : 32767;
  }
  const std::vector<uint8_t> original = MakeWavBytes(samples, 16000);
  WriteBytes(in, original);

  RunPassthrough(in, out);
  EXPECT_EQ(ReadBytes(out), original);
}

TEST(PassthroughPipeline, PreservesANonStandardSampleRate) {
  // The sample rate travels with the format rather than being hardcoded, so a
  // file that is not 16 kHz still round-trips.
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string out = dir.file("out.wav");

  const std::vector<int16_t> samples = MakeSweep(500, 48000);
  const std::vector<uint8_t> original = MakeWavBytes(samples, 48000);
  WriteBytes(in, original);

  RunPassthrough(in, out);
  EXPECT_EQ(ReadBytes(out), original);
}

TEST(PassthroughPipeline, RunningTwiceIsStable) {
  // Feeding the output back in must be a fixed point; that is what makes this
  // usable as a regression anchor across phases.
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string once = dir.file("once.wav");
  const std::string twice = dir.file("twice.wav");

  WriteBytes(in, MakeWavBytes(MakeSweep(777), 16000));
  RunPassthrough(in, once);
  RunPassthrough(once, twice);

  EXPECT_EQ(ReadBytes(once), ReadBytes(twice));
}

TEST(Pipeline, RejectsAMissingStage) {
  Pipeline pipeline;
  EXPECT_THROW(pipeline.run(), std::invalid_argument);

  TempDir dir;
  const std::string in = dir.file("in.wav");
  WriteBytes(in, MakeWavBytes(MakeSweep(64)));

  WavFileSource source(in);
  PassthroughEncoder encoder;
  LoopbackTransport transport;
  PassthroughDecoder decoder;
  pipeline.source = &source;
  pipeline.encoder = &encoder;
  pipeline.transport = &transport;
  pipeline.decoder = &decoder;
  EXPECT_THROW(pipeline.run(), std::invalid_argument);  // sink still null
}

// A transport that holds everything until close(), to prove the pipeline drains
// after the source is exhausted rather than assuming one-in-one-out. Phase 3
// relies on this when packets arrive late.
class BufferingTransport : public Transport {
 public:
  void send(const Packet& packet) override { queue_.push_back(packet); }

  bool receive(Packet& out) override {
    if (!closed_ || queue_.empty()) {
      return false;
    }
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  void close() override { closed_ = true; }

 private:
  std::deque<Packet> queue_;
  bool closed_ = false;
};

TEST(Pipeline, DrainsPacketsHeldUntilClose) {
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string out = dir.file("out.wav");
  const std::vector<uint8_t> original = MakeWavBytes(MakeSweep(300), 16000);
  WriteBytes(in, original);

  WavFileSource source(in);
  PassthroughEncoder encoder;
  BufferingTransport transport;
  PassthroughDecoder decoder;
  WavFileSink sink(out, source.format(), source.total_samples());

  Pipeline pipeline;
  pipeline.source = &source;
  pipeline.encoder = &encoder;
  pipeline.transport = &transport;
  pipeline.decoder = &decoder;
  pipeline.sink = &sink;

  const PipelineStats stats = pipeline.run();
  EXPECT_EQ(stats.packets_received, stats.packets_sent);
  EXPECT_EQ(ReadBytes(out), original);
}

}  // namespace
