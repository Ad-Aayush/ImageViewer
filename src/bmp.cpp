#include "bmp.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

bool BmpDecoder::canDecode(const std::vector<uint8_t> &buff) const {
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

BmpDecoder::BmpHeader
BmpDecoder::parseHeader(const std::vector<uint8_t> &buff) const {
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

bool BmpDecoder::decode(const std::vector<uint8_t> &buff, Image &out) const {
  std::cout << "Found BMP\n";
  BmpHeader header = parseHeader(buff);

  if (header.infoHeaderSz != 40) {
    std::cerr << "May not work as info header is not 40 bytes...\n";
  }

  if (header.height < 0) {
    std::cerr << "Top-down BMPs not supported...\n";
    return false;
  }
  out.setDimensions(header.width, header.height);
  out.reservePixels(static_cast<size_t>(header.width) * header.height * 4);

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
    return decode24Bit(buff, header, out);
  case 16:
    return decode16Bit(buff, header, out);
  case 8: {
    if (header.compression == 1) {
      return decodeRle8(buff, header, palette, out);
    }
    return decode8Bit(buff, header, palette, out);
  }
  case 4: {
    if (header.compression == 2) {
      return decodeRle4(buff, header, palette, out);
    }
    return decode4Bit(buff, header, palette, out);
  }
  case 1:
    return decode1Bit(buff, header, palette, out);
  default:
    std::cerr << "Not Supported yet...\n";
    return false;
  }
}

bool BmpDecoder::decode24Bit(const std::vector<uint8_t> &buff,
                             const BmpHeader &header, Image &out) const {
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

      out.pushPixel(RGBA(b, g, r));
    }
  }
  return true;
}

bool BmpDecoder::decode16Bit(const std::vector<uint8_t> &buff,
                             const BmpHeader &header, Image &out) const {
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

      out.pushPixel(bgr);
    }
  }
  return true;
}

bool BmpDecoder::decodeRle8(const std::vector<uint8_t> &buff,
                            const BmpHeader &header,
                            const std::vector<RGBA> &palette,
                            Image &out) const {
  std::cout << "Found RLE8\n";
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE8...\n";
    return false;
  }
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  out.fill(palette[0]);

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
          out.writePixelAtByteIndex(writeIdx, palette[curByte]);
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
        out.writePixelAtByteIndex(writeIdx, palette[idx]);
        x++;
      }
    }
  }
  std::cout << "Parsed RLE8\n";
  return true;
}

bool BmpDecoder::decode8Bit(const std::vector<uint8_t> &buff,
                            const BmpHeader &header,
                            const std::vector<RGBA> &palette,
                            Image &out) const {
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
      out.pushPixel(palette[buff[pixelIndex]]);
    }
  }
  return true;
}

bool BmpDecoder::decodeRle4(const std::vector<uint8_t> &buff,
                            const BmpHeader &header,
                            const std::vector<RGBA> &palette,
                            Image &out) const {
  if (palette.size() == 0) {
    std::cerr << "No palette found for RLE4...\n";
    return false;
  }
  std::cout << "Found RLE4\n";
  int x = 0, y = 0;
  int buffIdx = header.dataOffset;

  out.fill(palette[0]);

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
          out.writePixelAtByteIndex(writeIdx, palette[idx1]);
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
          out.writePixelAtByteIndex(writeIdx, palette[idx2]);
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
        out.writePixelAtByteIndex(writeIdx, palette[idx]);
        x++;
      }
    }
  }
  std::cout << "Parsed RLE4\n";
  return true;
}

bool BmpDecoder::decode4Bit(const std::vector<uint8_t> &buff,
                            const BmpHeader &header,
                            const std::vector<RGBA> &palette,
                            Image &out) const {
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
        out.pushPixel(palette[idx1]);
      } else {
        std::cerr << "Out of bounds color used...\n";
        return false;
      }

      // Low Nibble
      if (x + 1 < header.width) {
        int idx2 = byte & 0x0F;
        if ((size_t)idx2 < palette.size()) {
          out.pushPixel(palette[idx2]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}

bool BmpDecoder::decode1Bit(const std::vector<uint8_t> &buff,
                            const BmpHeader &header,
                            const std::vector<RGBA> &palette,
                            Image &out) const {
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
          out.pushPixel(palette[currIdx]);
        } else {
          std::cerr << "Out of bounds color used...\n";
          return false;
        }
      }
    }
  }
  return true;
}
