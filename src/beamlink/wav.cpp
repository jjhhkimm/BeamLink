#include "beamlink/wav.h"

#include <array>
#include <cstring>
#include <stdexcept>

namespace beamlink {
namespace {

// WAV is little-endian regardless of host byte order, so the integer accessors
// go through explicit byte shuffling rather than reinterpret_cast.
uint16_t ReadU16LE(const uint8_t* p) {
  return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t ReadU32LE(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

void WriteU16LE(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
}

void WriteU32LE(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v & 0xff);
  p[1] = static_cast<uint8_t>((v >> 8) & 0xff);
  p[2] = static_cast<uint8_t>((v >> 16) & 0xff);
  p[3] = static_cast<uint8_t>((v >> 24) & 0xff);
}

constexpr uint16_t kFormatPcm = 1;

void RequirePcm16Mono(const WavFormat& fmt, const std::string& path) {
  if (fmt.channels != 1) {
    throw std::runtime_error(path + ": expected mono, got " +
                             std::to_string(fmt.channels) + " channels");
  }
  if (fmt.bits_per_sample != 16) {
    throw std::runtime_error(path + ": expected 16-bit samples, got " +
                             std::to_string(fmt.bits_per_sample));
  }
}

}  // namespace

WavReader::WavReader(const std::string& path)
    : file_(path, std::ios::binary), path_(path) {
  if (!file_) {
    throw std::runtime_error(path + ": cannot open for reading");
  }

  std::array<uint8_t, 12> riff{};
  file_.read(reinterpret_cast<char*>(riff.data()), riff.size());
  if (file_.gcount() != static_cast<std::streamsize>(riff.size()) ||
      std::memcmp(riff.data(), "RIFF", 4) != 0 ||
      std::memcmp(riff.data() + 8, "WAVE", 4) != 0) {
    throw std::runtime_error(path + ": not a RIFF/WAVE file");
  }

  // Walk the chunk list rather than assuming a 44-byte header, so files with a
  // LIST or fact chunk still read correctly. Those will not round-trip
  // byte-identically, because the writer only emits the canonical header.
  bool have_fmt = false;
  for (;;) {
    std::array<uint8_t, 8> header{};
    file_.read(reinterpret_cast<char*>(header.data()), header.size());
    if (file_.gcount() != static_cast<std::streamsize>(header.size())) {
      throw std::runtime_error(path + ": truncated before a data chunk");
    }
    const uint32_t chunk_size = ReadU32LE(header.data() + 4);

    if (std::memcmp(header.data(), "data", 4) == 0) {
      if (!have_fmt) {
        throw std::runtime_error(path + ": data chunk precedes fmt chunk");
      }
      total_samples_ = chunk_size / sizeof(int16_t);
      break;  // Leave the stream positioned at the first sample.
    }

    if (std::memcmp(header.data(), "fmt ", 4) == 0) {
      if (chunk_size < 16) {
        throw std::runtime_error(path + ": fmt chunk is too small");
      }
      std::array<uint8_t, 16> fmt{};
      file_.read(reinterpret_cast<char*>(fmt.data()), fmt.size());
      if (file_.gcount() != static_cast<std::streamsize>(fmt.size())) {
        throw std::runtime_error(path + ": truncated fmt chunk");
      }
      if (ReadU16LE(fmt.data()) != kFormatPcm) {
        throw std::runtime_error(path + ": only uncompressed PCM is supported");
      }
      format_.channels = ReadU16LE(fmt.data() + 2);
      format_.sample_rate = ReadU32LE(fmt.data() + 4);
      format_.bits_per_sample = ReadU16LE(fmt.data() + 14);
      RequirePcm16Mono(format_, path);
      have_fmt = true;
      file_.seekg(chunk_size - 16, std::ios::cur);
    } else {
      file_.seekg(chunk_size, std::ios::cur);
    }

    // Chunks are word-aligned: an odd size is followed by a pad byte.
    if (chunk_size % 2 != 0) {
      file_.seekg(1, std::ios::cur);
    }
    if (!file_) {
      throw std::runtime_error(path + ": truncated chunk list");
    }
  }
}

size_t WavReader::read(int16_t* out, size_t count) {
  const uint64_t remaining = total_samples_ - samples_read_;
  const size_t want = static_cast<size_t>(
      remaining < count ? remaining : static_cast<uint64_t>(count));
  if (want == 0) {
    return 0;
  }

  file_.read(reinterpret_cast<char*>(out),
             static_cast<std::streamsize>(want * sizeof(int16_t)));
  const size_t got = static_cast<size_t>(file_.gcount()) / sizeof(int16_t);
  samples_read_ += got;

  // The data chunk claimed more samples than the file actually holds. Treat
  // that as end of stream rather than an error; truncated captures are common.
  if (got < want) {
    total_samples_ = samples_read_;
  }

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  for (size_t i = 0; i < got; ++i) {
    const uint16_t v = static_cast<uint16_t>(out[i]);
    out[i] = static_cast<int16_t>((v >> 8) | (v << 8));
  }
#endif
  return got;
}

WavWriter::WavWriter(const std::string& path, const WavFormat& format)
    : file_(path, std::ios::binary), path_(path), format_(format) {
  if (!file_) {
    throw std::runtime_error(path + ": cannot open for writing");
  }
  RequirePcm16Mono(format_, path);

  const uint32_t byte_rate =
      format_.sample_rate * format_.channels * (format_.bits_per_sample / 8u);
  const uint16_t block_align =
      static_cast<uint16_t>(format_.channels * (format_.bits_per_sample / 8u));

  // Sizes are placeholders; finish() patches them once the total is known.
  std::array<uint8_t, kWavHeaderBytes> header{};
  std::memcpy(header.data(), "RIFF", 4);
  WriteU32LE(header.data() + 4, 0);
  std::memcpy(header.data() + 8, "WAVE", 4);
  std::memcpy(header.data() + 12, "fmt ", 4);
  WriteU32LE(header.data() + 16, 16);
  WriteU16LE(header.data() + 20, kFormatPcm);
  WriteU16LE(header.data() + 22, format_.channels);
  WriteU32LE(header.data() + 24, format_.sample_rate);
  WriteU32LE(header.data() + 28, byte_rate);
  WriteU16LE(header.data() + 32, block_align);
  WriteU16LE(header.data() + 34, format_.bits_per_sample);
  std::memcpy(header.data() + 36, "data", 4);
  WriteU32LE(header.data() + 40, 0);

  file_.write(reinterpret_cast<const char*>(header.data()), header.size());
  if (!file_) {
    throw std::runtime_error(path + ": failed to write header");
  }
}

WavWriter::~WavWriter() {
  // Best effort on the destructor path; an explicit finish() reports errors.
  try {
    finish();
  } catch (const std::exception&) {
  }
}

void WavWriter::write(const int16_t* samples, size_t count) {
  if (finished_) {
    throw std::runtime_error(path_ + ": write after finish()");
  }
  if (count == 0) {
    return;
  }

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  std::vector<int16_t> swapped(samples, samples + count);
  for (size_t i = 0; i < count; ++i) {
    const uint16_t v = static_cast<uint16_t>(swapped[i]);
    swapped[i] = static_cast<int16_t>((v >> 8) | (v << 8));
  }
  samples = swapped.data();
#endif

  file_.write(reinterpret_cast<const char*>(samples),
              static_cast<std::streamsize>(count * sizeof(int16_t)));
  if (!file_) {
    throw std::runtime_error(path_ + ": failed to write samples");
  }
  samples_written_ += count;
}

void WavWriter::finish() {
  if (finished_) {
    return;
  }
  finished_ = true;

  const uint64_t data_bytes = samples_written_ * sizeof(int16_t);
  const uint64_t riff_bytes = data_bytes + kWavHeaderBytes - 8;
  if (riff_bytes > 0xffffffffull) {
    throw std::runtime_error(path_ + ": stream exceeds the 4 GiB WAV limit");
  }

  std::array<uint8_t, 4> buf{};
  WriteU32LE(buf.data(), static_cast<uint32_t>(riff_bytes));
  file_.seekp(4, std::ios::beg);
  file_.write(reinterpret_cast<const char*>(buf.data()), buf.size());

  WriteU32LE(buf.data(), static_cast<uint32_t>(data_bytes));
  file_.seekp(40, std::ios::beg);
  file_.write(reinterpret_cast<const char*>(buf.data()), buf.size());

  file_.flush();
  if (!file_) {
    throw std::runtime_error(path_ + ": failed to patch header sizes");
  }
  file_.close();
}

}  // namespace beamlink
