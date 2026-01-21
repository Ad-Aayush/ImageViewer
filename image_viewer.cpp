#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

extern "C" {
void render(const uint8_t *buffer, int width, int height);
}

struct RGBA {
  uint8_t b, g, r, a;

  RGBA() = default;
  RGBA(uint8_t b, uint8_t g, uint8_t r, uint8_t a = 255)
      : b(b), g(g), r(r), a(a) {}
};

void pushRgbaToVec(std::vector<uint8_t> &pixels, const RGBA color) {
  pixels.push_back(color.b);
  pixels.push_back(color.g);
  pixels.push_back(color.r);
  pixels.push_back(color.a);
}

void writeBgrToIdx(std::vector<uint8_t> &pixels, const RGBA color,
                   int32_t idx) {
  pixels[idx] = color.b;
  pixels[idx + 1] = color.g;
  pixels[idx + 2] = color.r;
  pixels[idx + 3] = color.a;
}

RGBA bgrFrom16bits(uint16_t curr) {
  uint8_t b = curr & ((1 << 5) - 1);
  uint8_t g = (curr >> 5) & ((1 << 5) - 1);
  uint8_t r = (curr >> 10) & ((1 << 5) - 1);

  // Expand 5-bit to 8-bit
  b = (b << 3) | (b >> 2);
  g = (g << 3) | (g >> 2);
  r = (r << 3) | (r >> 2);

  return RGBA(b, g, r);
}

struct BmpHeader {
  uint32_t fileSize;
  int32_t width;
  int32_t height;
  uint32_t dataOffset;
  uint16_t bitsPerPixel;
  uint32_t colorsUsed;
  uint32_t compression;
  uint32_t infoHeaderSz;
  int tableStart;
  int tableSz;
};

struct QoiHeader {
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  uint8_t colorSpace;
};

class Image {
public:
  Image() = default;

  bool load(const std::string &file);

  int getHeight() const { return m_height; }

  int getWidth() const { return m_width; }

  const uint8_t *getData() const { return m_pixels.data(); }

private:
  int m_height;
  int m_width;
  std::vector<uint8_t> m_pixels;

  bool isQoi(std::vector<uint8_t> &buff);
  bool parseQoi(std::vector<uint8_t> &buff);
  QoiHeader parseQoiHeader(std::vector<uint8_t> &buff);

  bool isBmp(std::vector<uint8_t> &buff);
  bool parseBmp(std::vector<uint8_t> &buff);
  BmpHeader parseBmpHeader(std::vector<uint8_t> &buff);

  bool decode24Bit(const std::vector<uint8_t> &buff, const BmpHeader &header);
  bool decode16Bit(const std::vector<uint8_t> &buff, const BmpHeader &header);
  bool decode8Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette);
  bool decodeRle8(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette);
  bool decode4Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette);
  bool decodeRle4(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette);
  bool decode1Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette);
};

std::optional<uint32_t> readNextByte(const std::vector<uint8_t> &buff,
                                     int &buffIdx) {
  int32_t byte;
  if ((size_t)buffIdx < buff.size()) {
    byte = buff[buffIdx++];
  } else {
    std::cerr << "Unexpted end of buffer...\n";
    return std::nullopt;
  }

  return byte;
}

bool Image::isBmp(std::vector<uint8_t> &buff) {
  if (buff.size() < 2) {
    return false;
  }
  if (buff[0] != 'B') {
    return false;
  }
  if (buff[1] != 'M') {
    return false;
  }
  return true;
}

bool Image::isQoi(std::vector<uint8_t> &buff) {
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

QoiHeader Image::parseQoiHeader(std::vector<uint8_t> &buff) {
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

bool Image::parseQoi(std::vector<uint8_t> &buff) {
  std::cout << "Found Qoi\n";
  QoiHeader header = parseQoiHeader(buff);

  if (header.channels != 3 && header.channels != 4) {
    return false;
  }
  if (header.colorSpace != 0 && header.colorSpace != 1) {
    return false;
  }

  m_height = header.height;
  m_width = header.width;

  m_pixels.reserve(m_width * m_height * 4);
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
  while (idx < sz && pixelsPushed < m_height * m_width) {
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
      pushRgbaToVec(m_pixels, bgr);
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
      pushRgbaToVec(m_pixels, bgr);
    } else if (firstTwoBits == 0) {
      uint8_t hashIdx = byte & 0x3f;
      RGBA color = hashArray[hashIdx];

      prev = color;
      pixelsPushed++;
      pushRgbaToVec(m_pixels, color);
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
      pushRgbaToVec(m_pixels, color);
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
      pushRgbaToVec(m_pixels, color);
    } else {
      uint8_t run = (byte & 0x3f) + 1;
      for (int i = 0; i < run && pixelsPushed < m_width * m_height; ++i) {
        pixelsPushed++;
        pushRgbaToVec(m_pixels, prev);
      }
    }
  }

  std::cout << "Parsed Qoi\n";
  return true;
}

BmpHeader Image::parseBmpHeader(std::vector<uint8_t> &buff) {
  BmpHeader header{};
  std::memcpy(&header.fileSize, &buff[2], 4);
  std::cout << "FileSize: " << header.fileSize << "\n";

  std::memcpy(&header.infoHeaderSz, &buff[14], 4);
  std::cout << "InfoHeaderSize: " << header.infoHeaderSz << "\n";

  std::memcpy(&header.width, &buff[18], 4);
  std::cout << "Width: " << header.width << "\n";

  std::memcpy(&header.height, &buff[22], 4);
  std::cout << "Height: " << header.height << "\n";

  std::memcpy(&header.dataOffset, &buff[10], 4);
  std::cout << "DataOffset: " << header.dataOffset << "\n";

  std::memcpy(&header.bitsPerPixel, &buff[28], 2);
  std::cout << "BitsPerPixel: " << header.bitsPerPixel << "\n";

  std::memcpy(&header.colorsUsed, &buff[46], 4);
  std::cout << "ColorsUsed: " << header.colorsUsed << "\n";

  std::memcpy(&header.compression, &buff[30], 4);
  std::cout << "Compression: " << header.compression << "\n";

  header.tableStart = 14 + header.infoHeaderSz;
  header.tableSz = (header.dataOffset - header.tableStart);
  std::cout << "TableSize: " << header.tableSz << "\n";

  return header;
}

bool Image::parseBmp(std::vector<uint8_t> &buff) {
  std::cout << "Found BMP\n";
  BmpHeader header = parseBmpHeader(buff);

  if (header.infoHeaderSz != 40) {
    std::cerr << "May not work as info header is not 40 bytes...\n";
  }

  if (header.height < 0) {
    std::cerr << "Top-down BMPs not supported...\n";
    return false;
  }

  m_width = header.width;
  m_height = header.height;
  m_pixels.reserve(m_width * m_height * 4);

  // Load Palette for indexed formats
  std::vector<RGBA> palette;
  if (header.bitsPerPixel <= 8) {
    int maxColors = 1 << header.bitsPerPixel;
    int givenColors = std::min(header.tableSz / 4, maxColors);

    for (int i = 0; i < givenColors; i++) {
      int idx = header.tableStart + (i * 4);
      uint8_t b = buff[idx];
      uint8_t g = buff[idx + 1];
      uint8_t r = buff[idx + 2];
      palette.emplace_back(b, g, r);
    }
  }

  switch (header.bitsPerPixel) {
  case 32:
  case 24:
    return decode24Bit(buff, header);
  case 16:
    return decode16Bit(buff, header);
  case 8: {
    if (header.compression == 1) {
      return decodeRle8(buff, header, palette);
    }
    return decode8Bit(buff, header, palette);
  }
  case 4: {
    if (header.compression == 2) {
      return decodeRle4(buff, header, palette);
    }
    return decode4Bit(buff, header, palette);
  }
  case 1:
    return decode1Bit(buff, header, palette);
  default:
    std::cerr << "Not Supported yet...\n";
    return false;
  }
}

bool Image::decode24Bit(const std::vector<uint8_t> &buff,
                        const BmpHeader &header) {
  if (header.compression != 0)
    return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;
  int bytesPerPx = header.bitsPerPixel / 8;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + (x * bytesPerPx);

      uint8_t b = buff[pixelIndex];
      uint8_t g = buff[pixelIndex + 1];
      uint8_t r = buff[pixelIndex + 2];

      pushRgbaToVec(m_pixels, RGBA(b, g, r));
    }
  }
  return true;
}

bool Image::decode16Bit(const std::vector<uint8_t> &buff,
                        const BmpHeader &header) {
  if (header.compression != 0)
    return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + (x * 2);

      uint16_t curr =
          (uint16_t)buff[pixelIndex] | ((uint16_t)buff[pixelIndex + 1] << 8);
      RGBA bgr = bgrFrom16bits(curr);

      pushRgbaToVec(m_pixels, bgr);
    }
  }
  return true;
}

bool Image::decodeRle8(const std::vector<uint8_t> &buff,
                       const BmpHeader &header,
                       const std::vector<RGBA> &palette) {
  std::cout << "Found RLE8\n";
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE8...\n";
    return false;
  }
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  for (int i = 0; i < m_width * m_height; ++i) {
    pushRgbaToVec(m_pixels, palette[0]);
  }

  while (true) {
    int32_t byte1 = readNextByte(buff, buffIdx).value_or(-1);
    int32_t byte2 = readNextByte(buff, buffIdx).value_or(-1);

    if (byte1 == -1 || byte2 == -1) {
      return false;
    }

    if (byte1 == 0 && byte2 == 1) {
      break;
    } else if (byte1 == 0 && byte2 == 0) {
      x = 0;
      y++;
      if (x < 0 || x > header.width || y < 0 || y >= header.height) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
    } else if (byte1 == 0 && byte2 == 2) {
      int32_t dx = readNextByte(buff, buffIdx).value_or(-1);
      int32_t dy = readNextByte(buff, buffIdx).value_or(-1);

      if (dx == -1 || dy == -1) {
        return false;
      }
      x += dx;
      y += dy;
      if (x < 0 || x > header.width || y < 0 || y >= header.height) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
    } else if (byte1 == 0) {
      // Absolute Mode
      int32_t len = byte2;
      if (x + len > header.width) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
      for (int i = 0; i < len; ++i) {
        int32_t curByte = readNextByte(buff, buffIdx).value_or(-1);
        if (curByte == -1) {
          return false;
        }
        if ((size_t)curByte < palette.size()) {
          int writeIdx = 4 * (x + (header.height - y - 1) * header.width);
          writeBgrToIdx(m_pixels, palette[curByte], writeIdx);
        } else {
          std::cerr << "Out of bounds palette access...\n";
          return false;
        }
        x++;
      }
      if (len % 2 != 0) {
        auto padByte = readNextByte(buff, buffIdx);
        if (!padByte.has_value()) {
          std::cerr << "Unexpected EOF...\n";
          return false;
        }
      }
    } else {
      int32_t cnt = byte1, idx = byte2;
      if ((size_t)idx >= palette.size()) {
        std::cerr << "Out of bounds...\n";
        return false;
      }
      if (x + cnt > header.width) {
        std::cerr << "Out of Bounds access...\n";
        return false;
      }

      for (int i = 0; i < cnt; i++) {
        int writeIdx = 4 * (x + (header.height - y - 1) * header.width);
        writeBgrToIdx(m_pixels, palette[idx], writeIdx);
        x++;
      }
    }
  }
  std::cout << "Parsed RLE8\n";
  return true;
}

bool Image::decode8Bit(const std::vector<uint8_t> &buff,
                       const BmpHeader &header,
                       const std::vector<RGBA> &palette) {
  if (palette.size() > 256) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0)
    return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + x;

      if (buff[pixelIndex] >= palette.size()) {
        std::cerr << "Out of bounds color used...\n";
        return false;
      }
      pushRgbaToVec(m_pixels, palette[buff[pixelIndex]]);
    }
  }
  return true;
}

bool Image::decodeRle4(const std::vector<uint8_t> &buff,
                       const BmpHeader &header,
                       const std::vector<RGBA> &palette) {
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE4...\n";
    return false;
  }
  std::cout << "Found RLE4\n";
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  for (int i = 0; i < m_width * m_height; ++i) {
    pushRgbaToVec(m_pixels, palette[0]);
  }

  while (true) {
    int32_t byte1 = readNextByte(buff, buffIdx).value_or(-1);
    int32_t byte2 = readNextByte(buff, buffIdx).value_or(-1);

    if (byte1 == -1 || byte2 == -1) {
      return false;
    }

    if (byte1 == 0 && byte2 == 1) {
      break;
    } else if (byte1 == 0 && byte2 == 0) {
      x = 0;
      y++;
      if (x < 0 || x > header.width || y < 0 || y >= header.height) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
    } else if (byte1 == 0 && byte2 == 2) {
      int32_t dx = readNextByte(buff, buffIdx).value_or(-1);
      int32_t dy = readNextByte(buff, buffIdx).value_or(-1);

      if (dx == -1 || dy == -1) {
        return false;
      }
      x += dx;
      y += dy;
      if (x < 0 || x > header.width || y < 0 || y >= header.height) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
    } else if (byte1 == 0) {
      // Absolute Mode
      int32_t len = byte2;
      if (x + len > header.width) {
        std::cerr << "Out of bounds access...\n";
        return false;
      }
      int32_t end = x + len;
      while (x < end) {
        int32_t curByte = readNextByte(buff, buffIdx).value_or(-1);
        if (curByte == -1) {
          return false;
        }

        // High Nibble
        int idx1 = (curByte >> 4) & 0x0F;
        if ((size_t)idx1 < palette.size()) {
          int writeIdx = 4 * (x + (header.height - y - 1) * header.width);
          writeBgrToIdx(m_pixels, palette[idx1], writeIdx);
        } else {
          std::cerr << "Out of bounds palette access...\n";
          return false;
        }
        x++;
        if (x >= end)
          break;

        // Low Nibble
        int idx2 = curByte & 0x0F;
        if ((size_t)idx2 < palette.size()) {
          int writeIdx = 4 * (x + (header.height - y - 1) * header.width);
          writeBgrToIdx(m_pixels, palette[idx2], writeIdx);
        } else {
          std::cerr << "Out of bounds palette access...\n";
          return false;
        }
        x++;
      }

      if (((len + 1) / 2) % 2 == 1) {
        auto padByte = readNextByte(buff, buffIdx);
        if (!padByte.has_value()) {
          std::cerr << "Unexpected EOF...\n";
          return false;
        }
      }
    } else {
      int32_t cnt = byte1;

      if (x + cnt > header.width) {
        std::cerr << "Out of Bounds access...\n";
        return false;
      }

      for (int i = 0; i < cnt; i++) {
        int idx;
        if (i % 2 == 0) {
          idx = (byte2 >> 4) & 0x0F;
        } else {
          idx = byte2 & 0x0F;
        }

        if ((size_t)idx >= palette.size()) {
          std::cerr << "Out of bounds...\n";
          return false;
        }

        int writeIdx = 4 * (x + (header.height - y - 1) * header.width);
        writeBgrToIdx(m_pixels, palette[idx], writeIdx);
        x++;
      }
    }
  }
  std::cout << "Parsed RLE4\n";
  return true;
}

bool Image::decode4Bit(const std::vector<uint8_t> &buff,
                       const BmpHeader &header,
                       const std::vector<RGBA> &palette) {
  if (palette.size() > 16) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0)
    return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; x += 2) {
      int pixelIndex = fileIndex + (x / 2);
      uint8_t byte = buff[pixelIndex];

      // High Nibble
      int idx1 = (byte >> 4) & 0x0F;
      if ((size_t)idx1 < palette.size()) {
        pushRgbaToVec(m_pixels, palette[idx1]);
      } else {
        std::cerr << "Out of bounds color used...\n";
        return false;
      }

      // Low Nibble
      if (x + 1 < header.width) {
        int idx2 = byte & 0x0F;
        if ((size_t)idx2 < palette.size()) {
          pushRgbaToVec(m_pixels, palette[idx2]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}

bool Image::decode1Bit(const std::vector<uint8_t> &buff,
                       const BmpHeader &header,
                       const std::vector<RGBA> &palette) {
  if (palette.size() > 2) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0)
    return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; x += 8) {
      int pixelIndex = fileIndex + (x / 8);
      uint8_t byte = buff[pixelIndex];

      for (int k = 7; k >= 0; k--) {
        if (x + 7 - k >= header.width)
          break;

        int currIdx = (byte >> k) & 1;
        if ((size_t)currIdx < palette.size()) {
          pushRgbaToVec(m_pixels, palette[currIdx]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}

bool Image::load(const std::string &file) {
  // Open the file at the end
  std::ifstream inp(file, std::ios::binary | std::ios::ate);
  if (!inp) {
    std::cerr << "Failed to open " << file << "\n";
    return false;
  }

  // tellg() gives the current read position which is the end here
  std::streamsize size = inp.tellg();

  // Go 0 bits forward from the start
  inp.seekg(0, std::ios::beg);

  std::vector<uint8_t> byteBuf(size);

  if (!inp.read(reinterpret_cast<char *>(byteBuf.data()), size)) {
    return false;
  }

  if (byteBuf.size() == 0) {
    return false;
  }

  if (isBmp(byteBuf)) {
    return parseBmp(byteBuf);
  }

  if (isQoi(byteBuf)) {
    return parseQoi(byteBuf);
  }
  return false;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <image-file>\n";
    return 1;
  }
  std::string filePath = argv[1];

  Image img;
  if (!img.load(filePath)) {
    return 1;
  }

  render(img.getData(), img.getWidth(), img.getHeight());

  return 0;
}
