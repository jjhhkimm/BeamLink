#include <gtest/gtest.h>

#include <stdexcept>

#include "beamlink/stages.h"
#include "test_support.h"

namespace {

using namespace beamlink;
using namespace beamlink_test;

TEST(FrameUnit, IsSixtyFourSamplesAtSixteenKilohertz) {
  // The frame unit is load-bearing for every later phase: 8 subbands x 8 blocks
  // is what the Phase 2 filterbank consumes. Pin it here so a change is a
  // deliberate, visible test edit rather than a silent drift.
  EXPECT_EQ(kSubbands, 8);
  EXPECT_EQ(kBlocksPerFrame, 8);
  EXPECT_EQ(kFrameSamples, 64);
  EXPECT_EQ(kSampleRate, 16000);
  EXPECT_EQ(kFrameDurationUs, 4000);
}

TEST(WavFileSource, SplitsIntoFramesAndNumbersThem) {
  TempDir dir;
  const std::string path = dir.file("in.wav");
  const std::vector<int16_t> samples = MakeSweep(kFrameSamples * 3);
  WriteBytes(path, MakeWavBytes(samples));

  WavFileSource source(path);
  EXPECT_EQ(source.total_samples(), samples.size());

  Frame frame;
  for (uint32_t f = 0; f < 3; ++f) {
    ASSERT_TRUE(source.next(frame));
    EXPECT_EQ(frame.sequence, f);
    for (int i = 0; i < kFrameSamples; ++i) {
      EXPECT_EQ(frame.samples[i], samples[f * kFrameSamples + i])
          << "frame " << f << " sample " << i;
    }
  }
  EXPECT_FALSE(source.next(frame));
}

TEST(WavFileSource, ZeroPadsThePartialFinalFrame) {
  TempDir dir;
  const std::string path = dir.file("in.wav");
  const std::vector<int16_t> samples = MakeSweep(kFrameSamples + 5);
  WriteBytes(path, MakeWavBytes(samples));

  WavFileSource source(path);
  Frame frame;
  ASSERT_TRUE(source.next(frame));
  ASSERT_TRUE(source.next(frame));
  EXPECT_EQ(frame.sequence, 1u);
  for (int i = 0; i < 5; ++i) {
    EXPECT_EQ(frame.samples[i], samples[kFrameSamples + i]);
  }
  for (int i = 5; i < kFrameSamples; ++i) {
    EXPECT_EQ(frame.samples[i], 0) << "padding at " << i;
  }
  EXPECT_FALSE(source.next(frame));
}

TEST(WavFileSource, EmptyFileYieldsNoFrames) {
  TempDir dir;
  const std::string path = dir.file("empty.wav");
  WriteBytes(path, MakeWavBytes({}));

  WavFileSource source(path);
  Frame frame;
  EXPECT_FALSE(source.next(frame));
  EXPECT_EQ(source.total_samples(), 0u);
}

TEST(PassthroughCodec, RoundTripPreservesEverySample) {
  PassthroughEncoder encoder;
  PassthroughDecoder decoder;

  Frame in;
  const std::vector<int16_t> samples = MakeSweep(kFrameSamples);
  for (int i = 0; i < kFrameSamples; ++i) {
    in.samples[i] = samples[i];
  }
  // Include the extremes explicitly; sign handling in the byte packing is the
  // easiest thing here to get wrong.
  in.samples[0] = -32768;
  in.samples[1] = 32767;
  in.samples[2] = -1;
  in.sequence = 42;

  Packet packet;
  encoder.encode(in, packet);
  EXPECT_EQ(packet.sequence, 42u);
  EXPECT_EQ(packet.payload.size(), kFrameSamples * sizeof(int16_t));

  Frame out;
  decoder.decode(packet, out);
  EXPECT_EQ(out.sequence, 42u);
  EXPECT_EQ(out.samples, in.samples);
}

TEST(PassthroughCodec, PayloadIsLittleEndian) {
  PassthroughEncoder encoder;
  Frame in;
  in.samples[0] = static_cast<int16_t>(0x1234);
  Packet packet;
  encoder.encode(in, packet);
  EXPECT_EQ(packet.payload[0], 0x34);
  EXPECT_EQ(packet.payload[1], 0x12);
}

TEST(PassthroughCodec, DecoderRejectsAWrongSizedPayload) {
  PassthroughDecoder decoder;
  Packet packet;
  packet.payload.resize(kFrameSamples * sizeof(int16_t) - 1);
  Frame out;
  EXPECT_THROW(decoder.decode(packet, out), std::runtime_error);
}

TEST(PassthroughCodec, EncoderReusesTheOutputPacket) {
  // The pipeline hands the same Packet to encode() every frame to avoid a
  // per-frame allocation, so encoding twice must not append.
  PassthroughEncoder encoder;
  Frame in;
  Packet packet;
  encoder.encode(in, packet);
  encoder.encode(in, packet);
  EXPECT_EQ(packet.payload.size(), kFrameSamples * sizeof(int16_t));
}

TEST(LoopbackTransport, DeliversInOrderAndReportsWhenEmpty) {
  LoopbackTransport transport;
  Packet out;
  EXPECT_FALSE(transport.receive(out));

  for (uint32_t i = 0; i < 3; ++i) {
    Packet p;
    p.sequence = i;
    p.payload.assign(4, static_cast<uint8_t>(i));
    transport.send(p);
  }
  for (uint32_t i = 0; i < 3; ++i) {
    ASSERT_TRUE(transport.receive(out));
    EXPECT_EQ(out.sequence, i);
    EXPECT_EQ(out.payload, std::vector<uint8_t>(4, static_cast<uint8_t>(i)));
  }
  EXPECT_FALSE(transport.receive(out));
}

TEST(LoopbackTransport, SendAfterCloseIsAnError) {
  LoopbackTransport transport;
  transport.close();
  EXPECT_THROW(transport.send(Packet{}), std::runtime_error);
}

TEST(WavFileSink, TrimsThePaddingOffTheFinalFrame) {
  TempDir dir;
  const std::string path = dir.file("out.wav");
  const uint64_t total = kFrameSamples + 5;

  Frame first;
  Frame second;
  const std::vector<int16_t> samples = MakeSweep(total);
  for (int i = 0; i < kFrameSamples; ++i) {
    first.samples[i] = samples[i];
  }
  for (int i = 0; i < 5; ++i) {
    second.samples[i] = samples[kFrameSamples + i];
  }

  {
    WavFileSink sink(path, WavFormat{16000, 1, 16}, total);
    sink.write(first);
    sink.write(second);
    // A stage downstream of a jitter buffer may over-deliver; extra frames past
    // the declared length must be dropped rather than lengthen the file.
    sink.write(second);
    sink.finish();
  }

  EXPECT_EQ(ReadBytes(path), MakeWavBytes(samples, 16000));
}

}  // namespace
