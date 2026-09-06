#pragma once

// Minimal WAV reader/writer, scoped deliberately narrowly: 16-bit mono PCM,
// which is the only format the pipeline carries. Anything else is rejected at
// parse time rather than silently mangled.
//
// The writer emits the canonical 44-byte RIFF header (RIFF / fmt / data, no
// extra chunks). Reading such a file and writing it straight back out is
// byte-for-byte identical, which is what makes the passthrough pipeline usable
// as a regression anchor.

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace beamlink {

struct WavFormat {
  uint32_t sample_rate = 0;
  uint16_t channels = 0;
  uint16_t bits_per_sample = 0;
};

// Size of the canonical header the writer emits.
inline constexpr size_t kWavHeaderBytes = 44;

// Streaming reader over a 16-bit mono PCM file.
class WavReader {
 public:
  // Throws std::runtime_error if the file is missing, malformed, or not
  // 16-bit mono PCM.
  explicit WavReader(const std::string& path);

  const WavFormat& format() const { return format_; }

  // Total number of samples in the data chunk.
  uint64_t total_samples() const { return total_samples_; }

  // Reads up to `count` samples into `out`. Returns the number actually read,
  // which is short only at end of stream.
  size_t read(int16_t* out, size_t count);

 private:
  std::ifstream file_;
  std::string path_;
  WavFormat format_;
  uint64_t total_samples_ = 0;
  uint64_t samples_read_ = 0;
};

// Writer for 16-bit mono PCM. The header is stamped with a placeholder size on
// construction and patched with the real byte counts by finish(), which the
// destructor also calls so a dropped writer still leaves a valid file.
class WavWriter {
 public:
  // Throws std::runtime_error if the file cannot be opened or the format is
  // not 16-bit mono PCM.
  WavWriter(const std::string& path, const WavFormat& format);
  ~WavWriter();

  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;

  void write(const int16_t* samples, size_t count);

  // Patches the RIFF and data chunk sizes and closes the file. Idempotent.
  void finish();

 private:
  std::ofstream file_;
  std::string path_;
  WavFormat format_;
  uint64_t samples_written_ = 0;
  bool finished_ = false;
};

}  // namespace beamlink
