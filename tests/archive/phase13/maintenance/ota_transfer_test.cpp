#include "OtaTransfer.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

static unsigned assertionCount = 0;
#undef assert
#define assert(condition) do { \
  ++assertionCount; \
  if (!(condition)) { \
    std::cerr << "CHECK failed at line " << __LINE__ << ": " #condition "\n"; \
    std::abort(); \
  } \
} while (0)

struct FakeUpdater {
  bool beginResult = true;
  bool endResult = true;
  std::size_t writeLimit = static_cast<std::size_t>(-1);
  unsigned beginCalls = 0;
  unsigned writeCalls = 0;
  unsigned endCalls = 0;
  unsigned abortCalls = 0;
  std::size_t begunSize = 0;
  std::vector<uint8_t> bytes;

  bool begin(std::size_t size) {
    ++beginCalls;
    begunSize = size;
    return beginResult;
  }

  std::size_t write(const uint8_t* data, std::size_t size) {
    ++writeCalls;
    const std::size_t written = size < writeLimit ? size : writeLimit;
    bytes.insert(bytes.end(), data, data + written);
    return written;
  }

  bool end() {
    ++endCalls;
    return endResult;
  }

  void abort() { ++abortCalls; }
};

static std::array<uint8_t, 24> validHeader() {
  std::array<uint8_t, 24> image{};
  image[0] = 0xe9;  // ESP image magic
  image[1] = 1;     // segment count: 1..16
  image[2] = 0;     // SPI mode: 0..3
  image[12] = 0;    // ESP32 chip ID, little endian
  image[13] = 0;
  return image;
}

static void invalidSizeAndBeginFaults() {
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(!transfer.start(23, 1024, 1));
    assert(!transfer.busy() && updater.beginCalls == 0);
  }
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(!transfer.start(25, 24, 1));
    assert(!transfer.busy() && updater.beginCalls == 0);
  }
  {
    FakeUpdater updater;
    updater.beginResult = false;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(!transfer.start(24, 24, 5));
    assert(!transfer.busy() && updater.beginCalls == 1 && updater.abortCalls == 1);
    // A failed begin leaves the conceptual updater inactive and reusable.
    updater.beginResult = true;
    assert(transfer.start(24, 24, 6));
    assert(transfer.busy() && updater.beginCalls == 2);
  }
}

static void validHeaderAndExactFinalChunk() {
  FakeUpdater updater;
  OtaTransfer::Transfer<FakeUpdater> transfer(updater);
  std::array<uint8_t, 1024> first{};
  const auto header = validHeader();
  for (std::size_t i = 0; i < header.size(); ++i) first[i] = header[i];
  std::array<uint8_t, 6> final{{1, 2, 3, 4, 5, 6}};

  assert(transfer.start(1030, 2048, 100));
  assert(updater.begunSize == 1030 && transfer.total() == 1030);
  assert(transfer.chunk(0, first.data(), first.size(), 101));
  assert(transfer.accepted() == 1024 && updater.writeCalls == 1);
  assert(transfer.chunk(1024, final.data(), final.size(), 102));
  assert(transfer.accepted() == 1030 && updater.writeCalls == 2);
  assert(transfer.finish(103));
  assert(!transfer.busy() && updater.endCalls == 1 && updater.abortCalls == 0);
  assert(updater.bytes.size() == 1030);
  assert(transfer.accepted() == 1030 && transfer.total() == 1030);
  assert(!transfer.finish(104));
  assert(updater.endCalls == 1);
}

static void rejectsMalformedHeaders() {
  for (unsigned which = 0; which < 6; ++which) {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    auto image = validHeader();
    switch (which) {
      case 0: image[0] = 0; break;
      case 1: image[1] = 0; break;
      case 2: image[1] = 17; break;
      case 3: image[2] = 4; break;
      case 4: image[12] = 1; break;
      case 5: image[13] = 1; break;
    }
    assert(transfer.start(24, 24, 0));
    assert(!transfer.chunk(0, image.data(), image.size(), 1));
    assert(!transfer.busy() && transfer.accepted() == 0);
    assert(updater.writeCalls == 0 && updater.endCalls == 0 && updater.abortCalls == 1);
  }

  // A short first request cannot provide the complete ESP image header.
  FakeUpdater updater;
  OtaTransfer::Transfer<FakeUpdater> transfer(updater);
  const uint8_t shortHeader[23] = {0xe9, 1, 0};
  assert(transfer.start(24, 24, 0));
  assert(!transfer.chunk(0, shortHeader, sizeof(shortHeader), 1));
  assert(updater.writeCalls == 0 && updater.abortCalls == 1 && !transfer.busy());
}

static void rejectsChunkShapeAndOrdering() {
  const auto image = validHeader();
  struct Case { std::size_t offset; const uint8_t* bytes; std::size_t size; };
  const Case invalid[] = {
      {0, image.data(), 0},                 // zero length
      {0, nullptr, image.size()},            // null data
      {0, image.data(), 1025},               // over per-request limit
      {1, image.data(), image.size()},        // gap at first chunk
      {0, image.data(), 25},                  // past declared total
  };
  for (const Case& item : invalid) {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(24, 24, 0));
    const uint8_t* data = item.bytes;
    std::size_t size = item.size;
    if (size == 1025) {
      static std::array<uint8_t, 1025> oversized{};
      oversized[0] = 0xe9; oversized[1] = 1;
      data = oversized.data();
    }
    if (size == 25) {
      static std::array<uint8_t, 25> tooLong{};
      tooLong[0] = 0xe9; tooLong[1] = 1;
      data = tooLong.data();
    }
    assert(!transfer.chunk(item.offset, data, size, 1));
    assert(!transfer.busy() && updater.abortCalls == 1 && updater.writeCalls == 0);
  }

  // Replayed offsets and partial truncation are aborted and cannot finalize.
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(48, 48, 0));
    assert(transfer.chunk(0, image.data(), image.size(), 1));
    assert(!transfer.chunk(0, image.data(), image.size(), 2));
    assert(!transfer.busy() && updater.abortCalls == 1 && updater.endCalls == 0);
  }
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(48, 48, 0));
    assert(transfer.chunk(0, image.data(), image.size(), 1));
    assert(!transfer.finish(2));
    assert(!transfer.busy() && updater.endCalls == 0 && updater.abortCalls == 1);
  }
}

static void backendWriteAndEndFaults() {
  const auto image = validHeader();
  {
    FakeUpdater updater;
    updater.writeLimit = image.size() - 1;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(image.size(), image.size(), 0));
    assert(!transfer.chunk(0, image.data(), image.size(), 1));
    assert(!transfer.busy() && updater.writeCalls == 1 && updater.abortCalls == 1);
    assert(transfer.accepted() == 0);
  }
  {
    FakeUpdater updater;
    updater.endResult = false;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(image.size(), image.size(), 0));
    assert(transfer.chunk(0, image.data(), image.size(), 1));
    assert(!transfer.finish(2));
    assert(!transfer.busy() && updater.endCalls == 1 && updater.abortCalls == 1);
  }
}

static void idleAndTotalExpiryBoundaries() {
  const auto image = validHeader();
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    assert(transfer.start(48, 48, 100));
    assert(transfer.tick(30099));  // 29,999 ms idle
    assert(transfer.busy() && updater.abortCalls == 0);
    assert(!transfer.tick(30100)); // exactly 30,000 ms idle
    assert(!transfer.busy() && updater.abortCalls == 1);
  }
  {
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    constexpr std::size_t total = 14u * 1024u + 24u;
    assert(transfer.start(total, total, 1000));
    // Refresh activity repeatedly so the 300 s total limit, rather than the
    // 30 s idle limit, is the condition that expires this transfer.
    std::array<uint8_t, 1024> block{};
    const auto header = validHeader();
    for (std::size_t i = 0; i < header.size(); ++i) block[i] = header[i];
    std::size_t offset = 0;
    for (unsigned i = 0; i < 14; ++i) {
      const uint32_t now = 2000u + i * 20000u;
      assert(transfer.chunk(offset, block.data(), block.size(), now));
      offset += block.size();
    }
    assert(transfer.chunk(offset, block.data(), 24, 282000));
    assert(offset + 24 == total);
    assert(transfer.tick(300999)); // 299,999 ms since start, less than 30 s idle
    assert(transfer.busy() && updater.abortCalls == 0);
    assert(!transfer.tick(301000)); // exactly 300,000 ms since start
    assert(!transfer.busy() && updater.abortCalls == 1);
  }
  {
    // Unsigned elapsed arithmetic also handles millis() wraparound.
    FakeUpdater updater;
    OtaTransfer::Transfer<FakeUpdater> transfer(updater);
    const uint32_t start = UINT32_MAX - 10000u;
    assert(transfer.start(48, 48, start));
    assert(transfer.tick(start + 29999u));
    assert(!transfer.tick(start + 30000u));
    assert(updater.abortCalls == 1);
  }
}

static void abortIsInactiveAndReusable() {
  FakeUpdater updater;
  OtaTransfer::Transfer<FakeUpdater> transfer(updater);
  assert(transfer.start(24, 24, 0));
  transfer.abort();
  assert(!transfer.busy() && updater.abortCalls == 1);
  transfer.abort();
  assert(updater.abortCalls == 1); // inactive abort is idempotent
  assert(transfer.start(24, 24, 10));
  assert(transfer.busy() && updater.beginCalls == 2);
}

int main() {
  invalidSizeAndBeginFaults();
  validHeaderAndExactFinalChunk();
  rejectsMalformedHeaders();
  rejectsChunkShapeAndOrdering();
  backendWriteAndEndFaults();
  idleAndTotalExpiryBoundaries();
  abortIsInactiveAndReusable();
  std::cout << "PASS: OTA transfer header, chunk, fault, sequence, completion, abort, and expiry cases ("
            << assertionCount << " checks)\n";
}
