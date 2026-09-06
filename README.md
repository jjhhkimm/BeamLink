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
