#pragma once

// Shared helpers for the test suite: a scratch directory that cleans itself up,
// plus raw byte-level WAV construction so tests can assert on exact file
// contents rather than going through the writer they are testing.

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace beamlink_test {

// A unique directory under the system temp dir, removed on destruction.
class TempDir {
 public:
  TempDir() {
    static int counter = 0;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch();
    path_ = std::filesystem::temp_directory_path() /
            ("beamlink_test_" +
             std::to_string(
                 std::chrono::duration_cast<std::chrono::nanoseconds>(stamp)
                     .count()) +
             "_" + std::to_string(counter++));
    std::filesystem::create_directories(path_);
  }

  ~TempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  TempDir(const TempDir&) = delete;
  TempDir& operator=(const TempDir&) = delete;

  std::string file(const std::string& name) const {
    return (path_ / name).string();
  }

 private:
  std::filesystem::path path_;
};

inline void AppendU16LE(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xff));
  out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
}

inline void AppendU32LE(std::vector<uint8_t>& out, uint32_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xff));
  out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
  out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
  out.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

// Builds a canonical 44-byte-header 16-bit mono WAV as raw bytes.
inline std::vector<uint8_t> MakeWavBytes(const std::vector<int16_t>& samples,
                                         uint32_t sample_rate = 16000) {
  const uint32_t data_bytes =
      static_cast<uint32_t>(samples.size() * sizeof(int16_t));
  std::vector<uint8_t> out;
  out.reserve(44 + data_bytes);

  out.insert(out.end(), {'R', 'I', 'F', 'F'});
  AppendU32LE(out, 36 + data_bytes);
  out.insert(out.end(), {'W', 'A', 'V', 'E'});
  out.insert(out.end(), {'f', 'm', 't', ' '});
  AppendU32LE(out, 16);
  AppendU16LE(out, 1);                 // PCM
  AppendU16LE(out, 1);                 // mono
  AppendU32LE(out, sample_rate);
  AppendU32LE(out, sample_rate * 2);   // byte rate
  AppendU16LE(out, 2);                 // block align
  AppendU16LE(out, 16);                // bits per sample
  out.insert(out.end(), {'d', 'a', 't', 'a'});
  AppendU32LE(out, data_bytes);
  for (int16_t s : samples) {
    AppendU16LE(out, static_cast<uint16_t>(s));
  }
  return out;
}

inline void WriteBytes(const std::string& path,
                       const std::vector<uint8_t>& bytes) {
  std::ofstream f(path, std::ios::binary);
  f.write(reinterpret_cast<const char*>(bytes.data()),
          static_cast<std::streamsize>(bytes.size()));
}

inline std::vector<uint8_t> ReadBytes(const std::string& path) {
  std::ifstream f(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(f),
                              std::istreambuf_iterator<char>());
}

// A deterministic sine sweep, so tests exercise real signal shapes rather than
// constants that would hide sample-ordering bugs.
inline std::vector<int16_t> MakeSweep(size_t count, uint32_t sample_rate = 16000,
                                      double f0 = 100.0, double f1 = 7000.0) {
  std::vector<int16_t> out(count);
  const double duration =
      count > 0 ? static_cast<double>(count) / sample_rate : 1.0;
  for (size_t i = 0; i < count; ++i) {
    const double t = static_cast<double>(i) / sample_rate;
    const double phase =
        2.0 * 3.14159265358979323846 * (f0 * t + 0.5 * (f1 - f0) / duration * t * t);
    out[i] = static_cast<int16_t>(std::lround(28000.0 * std::sin(phase)));
  }
  return out;
}

}  // namespace beamlink_test
