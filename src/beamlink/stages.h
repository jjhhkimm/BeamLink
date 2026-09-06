#pragma once

// Phase 1 implementations of the five stages. The WAV source and sink are real
// and stay; the encoder, transport, and decoder are no-op passthroughs that
// later phases replace.

#include <deque>
#include <memory>
#include <string>

#include "beamlink/stage.h"
#include "beamlink/wav.h"

namespace beamlink {

// Reads a WAV file one frame at a time, zero-padding the final frame.
class WavFileSource : public Source {
 public:
  explicit WavFileSource(const std::string& path);

  bool next(Frame& out) override;
  uint64_t total_samples() const override { return reader_.total_samples(); }
  WavFormat format() const override { return reader_.format(); }

 private:
  WavReader reader_;
  uint32_t next_sequence_ = 0;
};

// Serializes the frame samples little-endian. Phase 2 replaces this with the
// QMF analysis filterbank plus the quantizer and bit packer.
class PassthroughEncoder : public Encoder {
 public:
  void encode(const Frame& in, Packet& out) override;
};

// Hands packets straight back in send order, with no loss, delay, or reorder.
// Phase 3 replaces this with a real UDP socket sitting behind the channel
// simulator.
class LoopbackTransport : public Transport {
 public:
  void send(const Packet& packet) override;
  bool receive(Packet& out) override;
  void close() override { closed_ = true; }

 private:
  std::deque<Packet> queue_;
  bool closed_ = false;
};

// Mirror of PassthroughEncoder. Phase 2 replaces this with unpack, dequantize,
// and QMF synthesis.
class PassthroughDecoder : public Decoder {
 public:
  void decode(const Packet& in, Frame& out) override;
};

// Writes decoded frames to a WAV file, stopping at `total_samples` so the
// padding added to the final frame does not reach the file.
class WavFileSink : public Sink {
 public:
  WavFileSink(const std::string& path, const WavFormat& format,
              uint64_t total_samples);

  void write(const Frame& frame) override;
  void finish() override;

 private:
  WavWriter writer_;
  uint64_t remaining_samples_;
};

}  // namespace beamlink
