#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <vector>

extern "C" {
void render(const uint8_t* buffer, int width, int height);
}

struct BGRColor {
  uint8_t b, g, r;

  BGRColor(uint8_t b, uint8_t g, uint8_t r) : b(b), g(g), r(r) {}
};

void pushBgrToVec(std::vector<uint8_t>& pixels, const BGRColor color) {
  pixels.push_back(color.b);
  pixels.push_back(color.g);
  pixels.push_back(color.r);
  pixels.push_back(255);
}

void writeBgrToIdx(std::vector<uint8_t>& pixels, const BGRColor color,
                   int32_t idx) {
  pixels[idx] = color.b;
  pixels[idx + 1] = color.g;
  pixels[idx + 2] = color.r;
  pixels[idx + 3] = 255;
}

BGRColor bgrFrom16bits(uint16_t curr) {
  uint8_t b = curr & ((1 << 5) - 1);
  uint8_t g = (curr >> 5) & ((1 << 5) - 1);
  uint8_t r = (curr >> 10) & ((1 << 5) - 1);

  // Expand 5-bit to 8-bit
  b = (b << 3) | (b >> 2);
  g = (g << 3) | (g >> 2);
  r = (r << 3) | (r >> 2);

  return BGRColor(b, g, r);
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

class Image {
 public:
  Image() = default;

  bool load(const std::string& file);

  int getHeight() const { return m_height; }

  int getWidth() const { return m_width; }

  const uint8_t* getData() const { return m_pixels.data(); }

 private:
  int m_height;
  int m_width;
  std::vector<uint8_t> m_pixels;

  bool parseBmp(std::vector<uint8_t>& buff);
  BmpHeader parseBmpHeader(std::vector<uint8_t>& buff);

  bool decode24Bit(const std::vector<uint8_t>& buff, const BmpHeader& header);
  bool decode16Bit(const std::vector<uint8_t>& buff, const BmpHeader& header);
  bool decode8Bit(const std::vector<uint8_t>& buff, const BmpHeader& header,
                  const std::vector<BGRColor>& palette);
  bool decodeRle8(const std::vector<uint8_t>& buff, const BmpHeader& header,
                  const std::vector<BGRColor>& palette);
  bool decode4Bit(const std::vector<uint8_t>& buff, const BmpHeader& header,
                  const std::vector<BGRColor>& palette);
  bool decodeRle4(const std::vector<uint8_t>& buff, const BmpHeader& header,
                  const std::vector<BGRColor>& palette);
  bool decode1Bit(const std::vector<uint8_t>& buff, const BmpHeader& header,
                  const std::vector<BGRColor>& palette);
};

BmpHeader Image::parseBmpHeader(std::vector<uint8_t>& buff) {
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

bool Image::parseBmp(std::vector<uint8_t>& buff) {
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
  std::vector<BGRColor> palette;
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

bool Image::decode24Bit(const std::vector<uint8_t>& buff,
                        const BmpHeader& header) {
  if (header.compression != 0) return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;
  int bytesPerPx = header.bitsPerPixel / 8;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + (x * bytesPerPx);

      uint8_t b = buff[pixelIndex];
      uint8_t g = buff[pixelIndex + 1];
      uint8_t r = buff[pixelIndex + 2];

      pushBgrToVec(m_pixels, BGRColor(b, g, r));
    }
  }
  return true;
}

bool Image::decode16Bit(const std::vector<uint8_t>& buff,
                        const BmpHeader& header) {
  if (header.compression != 0) return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + (x * 2);

      uint16_t curr =
          (uint16_t)buff[pixelIndex] | ((uint16_t)buff[pixelIndex + 1] << 8);
      BGRColor bgr = bgrFrom16bits(curr);

      pushBgrToVec(m_pixels, bgr);
    }
  }
  return true;
}

std::optional<uint32_t> readNextByte(const std::vector<uint8_t>& buff,
                                     int& buffIdx) {
  int32_t byte;
  if ((size_t)buffIdx < buff.size()) {
    byte = buff[buffIdx++];
  } else {
    std::cerr << "Unexpted end of buffer...\n";
    return std::nullopt;
  }

  return byte;
}

bool Image::decodeRle8(const std::vector<uint8_t>& buff,
                       const BmpHeader& header,
                       const std::vector<BGRColor>& palette) {
  std::cout << "Found RLE8\n";
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE8...\n";
    return false;
  }
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  for (int i = 0; i < m_width * m_height; ++i) {
    pushBgrToVec(m_pixels, palette[0]);
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

bool Image::decode8Bit(const std::vector<uint8_t>& buff,
                       const BmpHeader& header,
                       const std::vector<BGRColor>& palette) {
  if (palette.size() > 256) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0) return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; ++x) {
      int pixelIndex = fileIndex + x;

      if (buff[pixelIndex] >= palette.size()) {
        std::cerr << "Out of bounds color used...\n";
        return false;
      }
      pushBgrToVec(m_pixels, palette[buff[pixelIndex]]);
    }
  }
  return true;
}

bool Image::decodeRle4(const std::vector<uint8_t>& buff,
                       const BmpHeader& header,
                       const std::vector<BGRColor>& palette) {
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE4...\n";
    return false;
  }
  std::cout << "Found RLE4\n";
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  for (int i = 0; i < m_width * m_height; ++i) {
    pushBgrToVec(m_pixels, palette[0]);
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
        if (x >= end) break;

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

bool Image::decode4Bit(const std::vector<uint8_t>& buff,
                       const BmpHeader& header,
                       const std::vector<BGRColor>& palette) {
  if (palette.size() > 16) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0) return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; x += 2) {
      int pixelIndex = fileIndex + (x / 2);
      uint8_t byte = buff[pixelIndex];

      // High Nibble
      int idx1 = (byte >> 4) & 0x0F;
      if ((size_t)idx1 < palette.size()) {
        pushBgrToVec(m_pixels, palette[idx1]);
      } else {
        std::cerr << "Out of bounds color used...\n";
        return false;
      }

      // Low Nibble
      if (x + 1 < header.width) {
        int idx2 = byte & 0x0F;
        if ((size_t)idx2 < palette.size()) {
          pushBgrToVec(m_pixels, palette[idx2]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}

bool Image::decode1Bit(const std::vector<uint8_t>& buff,
                       const BmpHeader& header,
                       const std::vector<BGRColor>& palette) {
  if (palette.size() > 2) {
    std::cerr << "Palette too large...\n";
    return false;
  }
  if (header.compression != 0) return false;

  int paddedRowSz = ((header.width * header.bitsPerPixel + 31) / 32) * 4;

  for (int y = 0; y < header.height; ++y) {
    int fileIndex = header.dataOffset + ((header.height - 1 - y) * paddedRowSz);
    for (int x = 0; x < header.width; x += 8) {
      int pixelIndex = fileIndex + (x / 8);
      uint8_t byte = buff[pixelIndex];

      for (int k = 7; k >= 0; k--) {
        if (x + 7 - k >= header.width) break;

        int currIdx = (byte >> k) & 1;
        if ((size_t)currIdx < palette.size()) {
          pushBgrToVec(m_pixels, palette[currIdx]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}

bool Image::load(const std::string& file) {
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

  if (!inp.read(reinterpret_cast<char*>(byteBuf.data()), size)) {
    return false;
  }

  if (byteBuf.size() == 0) {
    return false;
  }

  if (byteBuf.size() >= 2 && byteBuf[0] == 'B' && byteBuf[1] == 'M') {
    return parseBmp(byteBuf);
  }

  return false;
}

int main(int argc, char* argv[]) {
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