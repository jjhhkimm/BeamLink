# beamlink

A low-latency audio link: subband codec, UDP transport with a reproducible
channel simulator, jitter buffer, and packet loss concealment.

**Status: Phase 1 complete.** The full `Source -> Encoder -> Transport ->
Decoder -> Sink` path runs end to end with every stage a no-op passthrough, so
`beamlink in.wav out.wav` reproduces its input byte for byte. That identity is
the regression anchor for every phase after.

## Build and run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

python3 tools/make_test_wav.py in.wav
./build/src/beamlink in.wav out.wav
cmp in.wav out.wav          # exits 0: bit-identical
./build/bench/beamlink_bench
```

Requires CMake 3.16+ and a C++17 compiler. GoogleTest and Google Benchmark are
pulled in by FetchContent at configure time, so the first configure needs
network access. `-DBEAMLINK_BUILD_TESTS=OFF` and `-DBEAMLINK_BUILD_BENCHMARKS=OFF`
skip those dependencies.

## Layout

| Path | What it holds |
| --- | --- |
| [src/beamlink/frame.h](src/beamlink/frame.h) | The frame unit: 64 samples (8 subbands x 8 blocks) at 16 kHz, 4 ms. Fixed, not renegotiated. |
| [src/beamlink/wav.h](src/beamlink/wav.h) | 16-bit mono PCM reader/writer. Canonical 44-byte header on write. |
| [src/beamlink/stage.h](src/beamlink/stage.h) | The five abstract stage interfaces. |
| [src/beamlink/stages.h](src/beamlink/stages.h) | Phase 1 implementations: WAV source/sink, passthrough codec, loopback transport. |
| [src/beamlink/pipeline.h](src/beamlink/pipeline.h) | Pumps frames through the stages. Interleaves send and drain so an async transport still works. |
| [tests/](tests/) | GoogleTest suite, including the bit-identity acceptance test. |
| [bench/](bench/) | Google Benchmark baselines for per-frame cost. |
| [tools/](tools/) | `make_test_wav.py`, which emits a deterministic sine sweep. |

Each later phase replaces exactly one stage. The codec goes behind
`Encoder`/`Decoder`, UDP plus the channel simulator behind `Transport`, and the
jitter buffer and PLC in front of `Decoder` — none of which the other stages or
the pipeline need to know about.

## Roadmap

Phase 0 — Scaffold (30 min)

CMake project, C++17, src/ tests/ bench/. GoogleTest + Google Benchmark via FetchContent. GitHub Actions running build + tests on push.

Done when: empty test suite passes in CI.

Phase 1 — WAV I/O and a passthrough pipeline (half day)

Minimal WAV reader/writer for 16-bit mono PCM. Define your frame unit now and don't change it: 64 samples (8 subbands × 8 blocks), 16 kHz to start.

Build the pipeline as explicit stages with a common interface — Source → Encoder → Transport → Decoder → Sink — where every stage is a no-op passthrough for now.

Done when: beamlink in.wav out.wav produces a bit-identical file. This is your regression anchor for every phase after.

Phase 2 — The codec (2-3 days, the hard part)

Split this into two sessions or Claude Code will produce something plausible that sounds like static.

2a — Filterbank only, no quantization. 8-band polyphase QMF analysis + synthesis. Perfect-reconstruction check: analysis → synthesis with no quantization should return the input within float epsilon.

Acceptance: SNR > 90 dB on a sine sweep round-trip. If it isn't near-perfect here, stop and fix it — every later bug will look like a codec bug.

2b — Quantization. Per-subband scale factors, SNR-mode bit allocation, bitstream pack/unpack with a proper bit-level writer/reader.

Acceptance: round-trip SNR > 20 dB at ~128 kbps on a real speech or music clip. Write a tools/snr.py (or C++) that prints SNR, and commit the test clip.

Gotcha: the bit packer is where you'll lose a day. Test it in isolation — write 3 bits, 11 bits, 5 bits, read them back — before wiring it to the codec.

Phase 3 — Transport and channel simulator (1 day)

Packet header: sequence number, timestamp, payload length. Real UDP sockets over loopback (not an in-memory fake — you want the syscall cost in your latency numbers).

Channel simulator sits in front of the socket: configurable loss rate, jitter distribution, reorder probability, seeded RNG. Reproducibility matters more than realism — your benchmark numbers are worthless if they can't be re-run.

Done when: --loss 0.05 --jitter 20ms --seed 42 gives identical output across runs.

Phase 4 — Jitter buffer and PLC (1-2 days)

Ring of frame slots keyed by sequence number. Target depth adapts from observed arrival deviation (95th percentile over a sliding window), clamped to a min/max.

PLC on a gap: repeat the last decoded frame with a 2-4 ms crossfade, attenuate progressively across consecutive losses, mute after ~60 ms. Log every concealment event — you need the counts for the README.

Done when: at 5% loss, output is continuous with no clicks, and SNR degrades gracefully rather than falling off a cliff.

Phase 5 — Benchmarks and README (1 day)

This phase is the resume bullet. Don't skip it because the code already works.

Google Benchmark: encode and decode cost per frame, in µs
Harness sweeping loss 0 → 10%, recording SNR at each point
End-to-end latency budget broken down by stage (frame fill, encode, network, jitter buffer, decode)
README with an architecture diagram and 2-3 plots

Then fill in the XX values in your bullet from these numbers.

Optional Phase 6 — NEON

Only if you have time before the referral goes in. Hand-vectorize the filterbank inner loop with ARM NEON intrinsics, keep the scalar path behind a flag, report the speedup. Needs an ARM machine — Apple Silicon works, or a Pi.
