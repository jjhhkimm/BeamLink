// Baseline per-frame costs for the passthrough stages. These numbers are the
// floor that Phase 5 measures the real codec against: whatever encode() costs
// after Phase 2, this is the overhead that was already there.

#include <benchmark/benchmark.h>

#include <vector>

#include "beamlink/stages.h"

namespace {

beamlink::Frame MakeFrame() {
  beamlink::Frame frame;
  for (int i = 0; i < beamlink::kFrameSamples; ++i) {
    frame.samples[i] = static_cast<int16_t>((i * 811) % 32768 - 16384);
  }
  return frame;
}

void BM_Encode(benchmark::State& state) {
  beamlink::PassthroughEncoder encoder;
  const beamlink::Frame frame = MakeFrame();
  beamlink::Packet packet;

  for (auto _ : state) {
    encoder.encode(frame, packet);
    benchmark::DoNotOptimize(packet.payload.data());
  }
  state.SetItemsProcessed(state.iterations() * beamlink::kFrameSamples);
}
BENCHMARK(BM_Encode);

void BM_Decode(benchmark::State& state) {
  beamlink::PassthroughEncoder encoder;
  beamlink::PassthroughDecoder decoder;
  beamlink::Packet packet;
  encoder.encode(MakeFrame(), packet);
  beamlink::Frame out;

  for (auto _ : state) {
    decoder.decode(packet, out);
    benchmark::DoNotOptimize(out.samples.data());
  }
  state.SetItemsProcessed(state.iterations() * beamlink::kFrameSamples);
}
BENCHMARK(BM_Decode);

// Encode plus transport plus decode, which is the per-frame cost the latency
// budget in Phase 5 is built from.
void BM_EncodeTransportDecode(benchmark::State& state) {
  beamlink::PassthroughEncoder encoder;
  beamlink::PassthroughDecoder decoder;
  const beamlink::Frame frame = MakeFrame();
  beamlink::Packet packet;
  beamlink::Packet received;
  beamlink::Frame out;

  for (auto _ : state) {
    beamlink::LoopbackTransport transport;
    encoder.encode(frame, packet);
    transport.send(packet);
    if (transport.receive(received)) {
      decoder.decode(received, out);
    }
    benchmark::DoNotOptimize(out.samples.data());
  }
  state.SetItemsProcessed(state.iterations() * beamlink::kFrameSamples);
}
BENCHMARK(BM_EncodeTransportDecode);

}  // namespace

BENCHMARK_MAIN();
