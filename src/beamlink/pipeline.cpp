#include "beamlink/pipeline.h"

#include <stdexcept>

namespace beamlink {

PipelineStats Pipeline::run() {
  if (source == nullptr || encoder == nullptr || transport == nullptr ||
      decoder == nullptr || sink == nullptr) {
    throw std::invalid_argument("Pipeline::run: every stage must be set");
  }

  PipelineStats stats;
  Frame frame;
  Packet packet;
  Frame decoded;

  // Send and drain are interleaved rather than run in two passes: once the
  // transport is a real socket it cannot buffer the whole stream, and the
  // receive side has to keep up with the send side.
  const auto drain = [&]() {
    Packet received;
    while (transport->receive(received)) {
      ++stats.packets_received;
      decoder->decode(received, decoded);
      sink->write(decoded);
      ++stats.frames_written;
    }
  };

  while (source->next(frame)) {
    ++stats.frames_read;
    encoder->encode(frame, packet);
    transport->send(packet);
    ++stats.packets_sent;
    drain();
  }

  transport->close();
  drain();
  sink->finish();
  return stats;
}

}  // namespace beamlink
