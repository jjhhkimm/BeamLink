#pragma once

// The five pipeline stages, as abstract interfaces:
//
//   Source -> Encoder -> Transport -> Decoder -> Sink
//
// Every stage is an explicit seam. Phase 1 fills all five with passthrough
// implementations so the whole path can be exercised end to end; each later
// phase swaps exactly one of them for the real thing (the QMF codec behind
// Encoder/Decoder, UDP plus the channel simulator behind Transport, the jitter
// buffer and PLC in front of Decoder) without any other stage noticing.

#include "beamlink/frame.h"
#include "beamlink/wav.h"

namespace beamlink {

// Produces frames until the stream is exhausted.
class Source {
 public:
  virtual ~Source() = default;

  // Fills `out` with the next frame and returns true, or returns false at end
  // of stream. A partial final frame is zero-padded to kFrameSamples.
  virtual bool next(Frame& out) = 0;

  // Exact sample count of the underlying stream, excluding any padding added
  // to the final frame. The sink uses this to trim the padding back off.
  virtual uint64_t total_samples() const = 0;

  virtual WavFormat format() const = 0;
};

// Turns a frame into a packet payload.
class Encoder {
 public:
  virtual ~Encoder() = default;
  virtual void encode(const Frame& in, Packet& out) = 0;
};

// Carries packets from the sender to the receiver. Split into send/receive
// rather than a single transform because Phase 3 makes delivery asynchronous,
// lossy, and reordering: one send may yield zero or several receives.
class Transport {
 public:
  virtual ~Transport() = default;

  virtual void send(const Packet& packet) = 0;

  // Pops the next available packet, or returns false if none is ready.
  virtual bool receive(Packet& out) = 0;

  // Signals that no further packets will be sent, so receive() can drain.
  virtual void close() = 0;
};

// Turns a packet payload back into a frame.
class Decoder {
 public:
  virtual ~Decoder() = default;
  virtual void decode(const Packet& in, Frame& out) = 0;
};

// Consumes decoded frames.
class Sink {
 public:
  virtual ~Sink() = default;
  virtual void write(const Frame& frame) = 0;
  virtual void finish() = 0;
};

}  // namespace beamlink
