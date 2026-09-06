#pragma once

// The frame is the fixed unit of work for every stage in the pipeline. It is
// fixed here once and deliberately never renegotiated: the Phase 2 filterbank
// is 8 subbands x 8 blocks, which is exactly 64 samples, and every later stage
// (packet payloads, jitter buffer slots, PLC crossfades) is sized in frames.
//
//   64 samples @ 16 kHz = 4 ms of audio per frame.

#include <array>
#include <cstdint>
#include <vector>

namespace beamlink {

inline constexpr int kSubbands = 8;
inline constexpr int kBlocksPerFrame = 8;
inline constexpr int kFrameSamples = kSubbands * kBlocksPerFrame;  // 64
inline constexpr int kSampleRate = 16000;
inline constexpr int kChannels = 1;
inline constexpr int kBitsPerSample = 16;

// Duration of one frame in microseconds: 4000 us.
inline constexpr int kFrameDurationUs = (kFrameSamples * 1000000) / kSampleRate;

// One frame of 16-bit mono PCM.
//
// A stream whose length is not a multiple of kFrameSamples is zero-padded into
// its final frame; the sink is told the true sample count separately so it can
// truncate the padding back off. That keeps the frame size invariant without
// making round-trips lossy at the tail.
struct Frame {
  std::array<int16_t, kFrameSamples> samples{};
  // Index of this frame from the start of the stream. Phase 3 reuses this as
  // the packet sequence number.
  uint32_t sequence = 0;
};

// One encoded frame on the wire. In Phase 1 the payload is just the frame's
// samples in little-endian byte order; Phase 2 replaces it with the bitstream.
struct Packet {
  uint32_t sequence = 0;
  std::vector<uint8_t> payload;
};

}  // namespace beamlink
