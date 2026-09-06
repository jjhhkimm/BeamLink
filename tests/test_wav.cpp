#include <gtest/gtest.h>

#include <stdexcept>

#include "beamlink/wav.h"
#include "test_support.h"

namespace {

using beamlink::WavFormat;
using beamlink::WavReader;
using beamlink::WavWriter;
using namespace beamlink_test;

TEST(WavReader, ReadsFormatAndSampleCount) {
  TempDir dir;
  const std::string path = dir.file("in.wav");
  const std::vector<int16_t> samples = {0, 1, -1, 32767, -32768, 1234};
  WriteBytes(path, MakeWavBytes(samples, 16000));

  WavReader reader(path);
  EXPECT_EQ(reader.format().sample_rate, 16000u);
  EXPECT_EQ(reader.format().channels, 1u);
  EXPECT_EQ(reader.format().bits_per_sample, 16u);
  EXPECT_EQ(reader.total_samples(), samples.size());

  std::vector<int16_t> got(samples.size());
  EXPECT_EQ(reader.read(got.data(), got.size()), samples.size());
  EXPECT_EQ(got, samples);
  EXPECT_EQ(reader.read(got.data(), got.size()), 0u);
}

TEST(WavReader, ReadsInChunksSmallerThanTheFile) {
  TempDir dir;
  const std::string path = dir.file("in.wav");
  const std::vector<int16_t> samples = MakeSweep(200);
  WriteBytes(path, MakeWavBytes(samples));

  WavReader reader(path);
  std::vector<int16_t> got;
  int16_t buf[64];
  size_t n = 0;
  while ((n = reader.read(buf, 64)) > 0) {
    got.insert(got.end(), buf, buf + n);
  }
  EXPECT_EQ(got, samples);
}

TEST(WavReader, SkipsUnknownChunks) {
  TempDir dir;
  const std::string path = dir.file("extra.wav");
  const std::vector<int16_t> samples = {10, 20, 30, 40};

  // Splice a LIST chunk with an odd payload length between fmt and data, so
  // both the skip and the word-alignment pad byte are exercised.
  std::vector<uint8_t> bytes = MakeWavBytes(samples);
  std::vector<uint8_t> list = {'L', 'I', 'S', 'T'};
  AppendU32LE(list, 3);
  list.insert(list.end(), {'a', 'b', 'c', 0});
  bytes.insert(bytes.begin() + 36, list.begin(), list.end());
  // Fix up the RIFF size for the bytes we spliced in.
  const uint32_t riff = static_cast<uint32_t>(bytes.size() - 8);
  bytes[4] = static_cast<uint8_t>(riff & 0xff);
  bytes[5] = static_cast<uint8_t>((riff >> 8) & 0xff);
  bytes[6] = static_cast<uint8_t>((riff >> 16) & 0xff);
  bytes[7] = static_cast<uint8_t>((riff >> 24) & 0xff);
  WriteBytes(path, bytes);

  WavReader reader(path);
  ASSERT_EQ(reader.total_samples(), samples.size());
  std::vector<int16_t> got(samples.size());
  ASSERT_EQ(reader.read(got.data(), got.size()), samples.size());
  EXPECT_EQ(got, samples);
}

TEST(WavReader, RejectsBadInput) {
  TempDir dir;

  EXPECT_THROW({ WavReader r(dir.file("missing.wav")); }, std::runtime_error);

  const std::string junk = dir.file("junk.wav");
  WriteBytes(junk, {'n', 'o', 't', 'a', 'w', 'a', 'v', 'e', 0, 0, 0, 0});
  EXPECT_THROW({ WavReader r(junk); }, std::runtime_error);

  // Stereo must be rejected rather than silently interleaved.
  const std::string stereo = dir.file("stereo.wav");
  std::vector<uint8_t> bytes = MakeWavBytes({1, 2, 3, 4});
  bytes[22] = 2;
  WriteBytes(stereo, bytes);
  EXPECT_THROW({ WavReader r(stereo); }, std::runtime_error);

  // So must 8-bit.
  const std::string eight = dir.file("eight.wav");
  bytes = MakeWavBytes({1, 2, 3, 4});
  bytes[34] = 8;
  WriteBytes(eight, bytes);
  EXPECT_THROW({ WavReader r(eight); }, std::runtime_error);
}

TEST(WavReader, TruncatedDataChunkStopsAtEndOfFile) {
  TempDir dir;
  const std::string path = dir.file("short.wav");
  std::vector<uint8_t> bytes = MakeWavBytes({1, 2, 3, 4, 5, 6, 7, 8});
  bytes.resize(bytes.size() - 6);  // Drop three samples from the tail.
  WriteBytes(path, bytes);

  WavReader reader(path);
  std::vector<int16_t> got(8);
  EXPECT_EQ(reader.read(got.data(), got.size()), 5u);
  EXPECT_EQ(reader.total_samples(), 5u);
}

TEST(WavWriter, ProducesTheCanonicalHeader) {
  TempDir dir;
  const std::string path = dir.file("out.wav");
  const std::vector<int16_t> samples = MakeSweep(37);

  {
    WavWriter writer(path, WavFormat{16000, 1, 16});
    writer.write(samples.data(), samples.size());
    writer.finish();
  }

  EXPECT_EQ(ReadBytes(path), MakeWavBytes(samples, 16000));
}

TEST(WavWriter, PatchesSizesFromTheDestructorIfFinishIsSkipped) {
  TempDir dir;
  const std::string path = dir.file("out.wav");
  const std::vector<int16_t> samples = {5, 6, 7};
  {
    WavWriter writer(path, WavFormat{16000, 1, 16});
    writer.write(samples.data(), samples.size());
  }
  EXPECT_EQ(ReadBytes(path), MakeWavBytes(samples, 16000));
}

TEST(WavWriter, EmptyStreamIsAValidZeroLengthFile) {
  TempDir dir;
  const std::string path = dir.file("empty.wav");
  {
    WavWriter writer(path, WavFormat{16000, 1, 16});
    writer.finish();
  }
  EXPECT_EQ(ReadBytes(path), MakeWavBytes({}, 16000));

  WavReader reader(path);
  EXPECT_EQ(reader.total_samples(), 0u);
}

TEST(WavRoundTrip, ReadThenWriteIsByteIdentical) {
  TempDir dir;
  const std::string in = dir.file("in.wav");
  const std::string out = dir.file("out.wav");
  const std::vector<int16_t> samples = MakeSweep(1000);
  WriteBytes(in, MakeWavBytes(samples, 16000));

  {
    WavReader reader(in);
    WavWriter writer(out, reader.format());
    std::vector<int16_t> buf(128);
    size_t n = 0;
    while ((n = reader.read(buf.data(), buf.size())) > 0) {
      writer.write(buf.data(), n);
    }
    writer.finish();
  }

  EXPECT_EQ(ReadBytes(in), ReadBytes(out));
}

}  // namespace
