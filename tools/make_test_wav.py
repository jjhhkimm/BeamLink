#!/usr/bin/env python3
"""Generate a deterministic 16-bit mono PCM WAV for manual and CI checks.

The default is a logarithmic sine sweep, which is the signal Phase 2a measures
perfect reconstruction against, so the same clip stays useful later.

  python3 tools/make_test_wav.py out.wav [--seconds 1.0] [--rate 16000]
"""

import argparse
import math
import struct


def canonical_wav(samples, sample_rate):
    """Serialize to the canonical 44-byte-header form the C++ writer emits."""
    data = struct.pack("<%dh" % len(samples), *samples)
    return b"".join([
        b"RIFF",
        struct.pack("<I", 36 + len(data)),
        b"WAVEfmt ",
        struct.pack("<IHHIIHH", 16, 1, 1, sample_rate, sample_rate * 2, 2, 16),
        b"data",
        struct.pack("<I", len(data)),
        data,
    ])


def sweep(count, sample_rate, f0, f1, amplitude):
    samples = []
    duration = count / sample_rate if count else 1.0
    for i in range(count):
        t = i / sample_rate
        phase = 2.0 * math.pi * (f0 * t + 0.5 * (f1 - f0) / duration * t * t)
        value = int(round(amplitude * math.sin(phase)))
        samples.append(max(-32768, min(32767, value)))
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output")
    parser.add_argument("--seconds", type=float, default=1.0)
    parser.add_argument("--rate", type=int, default=16000)
    parser.add_argument("--f0", type=float, default=100.0)
    parser.add_argument("--f1", type=float, default=7000.0)
    parser.add_argument("--amplitude", type=float, default=28000.0)
    args = parser.parse_args()

    count = int(args.seconds * args.rate)
    samples = sweep(count, args.rate, args.f0, args.f1, args.amplitude)
    with open(args.output, "wb") as f:
        f.write(canonical_wav(samples, args.rate))
    print("wrote %s: %d samples @ %d Hz" % (args.output, count, args.rate))


if __name__ == "__main__":
    main()
