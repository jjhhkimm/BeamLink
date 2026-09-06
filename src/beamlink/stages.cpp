#include "beamlink/stages.h"

#include <stdexcept>

namespace beamlink {

WavFileSource::WavFileSource(const std::string& path) : reader_(path) {}

bool WavFileSource::next(Frame& out) {
  out.samples.fill(0);
  const size_t got = reader_.read(out.samples.data(), kFrameSamples);
  if (got == 0) {
    return false;
  }
  // A short read only happens on the final frame; the fill above has already
  // zero-padded the tail.
  out.sequence = next_sequence_++;
  return true;
}

void PassthroughEncoder::encode(const Frame& in, Packet& out) {
  out.sequence = in.sequence;
  out.payload.resize(kFrameSamples * sizeof(int16_t));
  for (int i = 0; i < kFrameSamples; ++i) {
    const uint16_t v = static_cast<uint16_t>(in.samples[i]);
    out.payload[2 * i] = static_cast<uint8_t>(v & 0xff);
    out.payload[2 * i + 1] = static_cast<uint8_t>((v >> 8) & 0xff);
  }
}

void LoopbackTransport::send(const Packet& packet) {
  if (closed_) {
    throw std::runtime_error("LoopbackTransport: send after close()");
  }
  queue_.push_back(packet);
}

bool LoopbackTransport::receive(Packet& out) {
  if (queue_.empty()) {
    return false;
  }
  out = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

void PassthroughDecoder::decode(const Packet& in, Frame& out) {
  if (in.payload.size() != kFrameSamples * sizeof(int16_t)) {
    throw std::runtime_error("PassthroughDecoder: payload is " +
                             std::to_string(in.payload.size()) +
                             " bytes, expected " +
                             std::to_string(kFrameSamples * sizeof(int16_t)));
  }
  out.sequence = in.sequence;
  for (int i = 0; i < kFrameSamples; ++i) {
    const uint16_t v = static_cast<uint16_t>(in.payload[2 * i]) |
                       static_cast<uint16_t>(in.payload[2 * i + 1] << 8);
    out.samples[i] = static_cast<int16_t>(v);
  }
}

WavFileSink::WavFileSink(const std::string& path, const WavFormat& format,
                         uint64_t total_samples)
    : writer_(path, format), remaining_samples_(total_samples) {}

void WavFileSink::write(const Frame& frame) {
  if (remaining_samples_ == 0) {
    return;
  }
  const size_t count = static_cast<size_t>(
      remaining_samples_ < kFrameSamples
          ? remaining_samples_
          : static_cast<uint64_t>(kFrameSamples));
  writer_.write(frame.samples.data(), count);
  remaining_samples_ -= count;
}

void WavFileSink::finish() { writer_.finish(); }

}  // namespace beamlink
