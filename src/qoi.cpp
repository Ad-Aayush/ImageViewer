#include "qoi.h"
#include <array>
#include <iostream>

QoiDecoder::QoiHeader
QoiDecoder::parseHeader(const std::vector<uint8_t> &buff) const {
  QoiHeader header{};

  std::cout << "Buff.size(): " << buff.size() << "\n";
  for (int i = 4; i < 8; ++i) {
    header.width = (header.width << 8) | (uint32_t)buff[i];
  }
  std::cout << "Width: " << header.width << "\n";

  for (int i = 8; i < 12; ++i) {
    header.height = (header.height << 8) | (uint32_t)buff[i];
  }
  std::cout << "Height: " << header.height << "\n";

  header.channels = buff[12];
  std::cout << "Channels: " << (uint32_t)header.channels << "\n";

  header.colorSpace = buff[13];
  std::cout << "ColorSpace: " << (uint32_t)header.colorSpace << "\n";

  return header;
}

bool QoiDecoder::canDecode(const std::vector<uint8_t> &buff) const {
  if (buff.size() < 22) {
    return false;
  }
  if (buff[0] != 'q') {
    return false;
  }
  if (buff[1] != 'o') {
    return false;
  }
  if (buff[2] != 'i') {
    return false;
  }
  if (buff[3] != 'f') {
    return false;
  }

  int sz = buff.size();
  if (buff[sz - 1] != 0x01) {
    return false;
  }

  for (int i = sz - 2; i >= sz - 8; i--) {
    if (buff[i] != 0x00) {
      return false;
    }
  }

  return true;
}

bool QoiDecoder::decode(const std::vector<uint8_t> &buff, Image &out) const {
  std::cout << "Found Qoi\n";
  QoiHeader header = parseHeader(buff);

  if (header.channels != 3 && header.channels != 4) {
    return false;
  }
  if (header.colorSpace != 0 && header.colorSpace != 1) {
    return false;
  }

  out.setDimensions(header.width, header.height);

  out.reservePixels(static_cast<size_t>(header.width) * header.height * 4);
  std::array<RGBA, 64> hashArray;
  for (auto &c : hashArray) {
    c = RGBA(0, 0, 0, 255);
  }

  auto hash = [](const RGBA &color) {
    return (3 * color.r + 5 * color.g + 7 * color.b + 11 * color.a) % 64;
  };

  int idx = 14;
  int sz = buff.size() - 8;
  int pixelsPushed = 0;
  RGBA prev = {0, 0, 0, 255};
  while (idx < sz && pixelsPushed < (int)(header.height * header.width)) {
    auto maybeByte = readNextByte(buff, idx);
    if (!maybeByte.has_value()) {
      return false;
    }
    uint32_t byte = maybeByte.value();
    uint8_t firstTwoBits = (byte & 0xc0) >> 6;

    if (byte == 0xfe) {
      auto maybeR = readNextByte(buff, idx), maybeG = readNextByte(buff, idx),
           maybeB = readNextByte(buff, idx);
      if (!maybeR.has_value() || !maybeG.has_value() || !maybeB.has_value()) {
        return false;
      }
      RGBA bgr(maybeB.value(), maybeG.value(), maybeR.value(), prev.a);

      int curHash = hash(bgr);
      hashArray[curHash] = bgr;

      prev = bgr;
      pixelsPushed++;
      out.pushPixel(bgr);
    } else if (byte == 0xff) {
      auto maybeR = readNextByte(buff, idx), maybeG = readNextByte(buff, idx),
           maybeB = readNextByte(buff, idx), maybeA = readNextByte(buff, idx);
      if (!maybeR.has_value() || !maybeG.has_value() || !maybeB.has_value() ||
          !maybeA.has_value()) {
        return false;
      }
      RGBA bgr(maybeB.value(), maybeG.value(), maybeR.value(), maybeA.value());

      int curHash = hash(bgr);
      hashArray[curHash] = bgr;

      prev = bgr;
      pixelsPushed++;
      out.pushPixel(bgr);
    } else if (firstTwoBits == 0) {
      uint8_t hashIdx = byte & 0x3f;
      RGBA color = hashArray[hashIdx];

      prev = color;
      pixelsPushed++;
      out.pushPixel(color);
    } else if (firstTwoBits == 1) {
      int8_t dr = (byte & 0x30) >> 4;
      int8_t dg = (byte & 0x0c) >> 2;
      int8_t db = (byte & 0x03);

      uint8_t r = prev.r + dr - 2;
      uint8_t g = prev.g + dg - 2;
      uint8_t b = prev.b + db - 2;

      RGBA color(b, g, r, prev.a);

      int curHash = hash(color);
      hashArray[curHash] = color;

      prev = color;
      pixelsPushed++;
      out.pushPixel(color);
    } else if (firstTwoBits == 2) {
      int dg = (int)(byte & 0x3f) - 32;

      auto maybeNextByte = readNextByte(buff, idx);
      if (!maybeNextByte.has_value()) {
        return false;
      }
      uint8_t nextByte = maybeNextByte.value();

      int dr_dg = (int)((nextByte & 0xf0) >> 4) - 8;
      int db_dg = (int)(nextByte & 0x0f) - 8;

      uint8_t g = prev.g + dg;
      uint8_t r = prev.r + dr_dg + dg;
      uint8_t b = prev.b + db_dg + dg;

      RGBA color(b, g, r, prev.a);

      int curHash = hash(color);
      hashArray[curHash] = color;

      prev = color;
      pixelsPushed++;
      out.pushPixel(color);
    } else {
      uint8_t run = (byte & 0x3f) + 1;
      for (int i = 0; i < run && pixelsPushed < (int)(header.width * header.height); ++i) {
        pixelsPushed++;
        out.pushPixel(prev);
      }
    }
  }

  std::cout << "Parsed Qoi\n";
  return true;
}
