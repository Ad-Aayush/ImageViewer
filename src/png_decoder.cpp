#include "png_decoder.h"
#include <iostream>

bool PngDecoder::canDecode(const std::vector<uint8_t> &buff) const {
  if (buff.size() < 8) {
    return false;
  }
  if (buff[0] != 0x89) {
    return false;
  }
  if (buff[1] != 0x50) {
    return false;
  }
  if (buff[2] != 0x4E) {
    return false;
  }
  if (buff[3] != 0x47) {
    return false;
  }
  if (buff[4] != 0x0D) {
    return false;
  }
  if (buff[5] != 0x0A) {
    return false;
  }
  if (buff[6] != 0x1A) {
    return false;
  }
  if (buff[7] != 0x0A) {
    return false;
  }
  return true;
}

std::optional<uint32_t>
PngDecoder::read4BytesAsU32(const std::vector<uint8_t> &buff,
                            int &idx) const {
  uint32_t ans = 0;
  if (idx + 4 > (int)buff.size()) {
    std::cerr << "Unexpected End of file...\n";
    return std::nullopt;
  }

  int cnt = 0;

  while (cnt < 4) {
    ans = (ans << 8) | (uint32_t)buff[idx++];
    cnt++;
  }

  return ans;
}

std::optional<std::string>
PngDecoder::read4BytesAsStr(const std::vector<uint8_t> &buff,
                            int &idx) const {
  std::string ans;
  if (idx + 4 > (int)buff.size()) {
    std::cerr << "Unexpected End of file...\n";
    return std::nullopt;
  }

  int cnt = 0;

  while (cnt < 4) {
    ans += (char)buff[idx++];
    cnt++;
  }

  return ans;
}

bool PngDecoder::parsePng(const std::vector<uint8_t> &buff) const {
  std::cout << "Found Png...\n";
  int idx = 8;

  while (true) {
    std::cout << "Reading Chunk...\n";
    auto maybeLen = read4BytesAsU32(buff, idx);

    if (!maybeLen.has_value()) {
      return false;
    }

    uint32_t len = maybeLen.value();
    std::cout << "Length: " << len << "\n";

    auto maybeChunkType = read4BytesAsStr(buff, idx);

    if (!maybeChunkType.has_value()) {
      return false;
    }

    std::string chunkType = maybeChunkType.value();
    std::cout << "ChunkType: " << chunkType << "\n";

    idx += len;

    auto maybeCrc = read4BytesAsU32(buff, idx);

    if (!maybeCrc.has_value()) {
      return false;
    }

    uint32_t crc = maybeCrc.value();
    std::cout << "Crc: " << crc << "\n";

    if (chunkType == "IEND") {
      break;
    }
  }

  return true;
}

bool PngDecoder::decode(const std::vector<uint8_t> &buff, Image &out) const {
  out.setDimensions(0, 0);
  out.reservePixels(0);
  return parsePng(buff);
}
