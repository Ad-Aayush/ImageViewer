#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
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

BGRColor bgrFrom16bits(uint16_t curr) {
  uint8_t b = curr & ((1 << 5) - 1);
  uint8_t g = (curr >> 5) & ((1 << 5) - 1);
  uint8_t r = (curr >> 10) & ((1 << 5) - 1);

  // I now have 5 bit colors, I need 8 bits.
  b = (b << 3) | (b >> 2);
  g = (g << 3) | (g >> 2);
  r = (r << 3) | (r >> 2);

  return BGRColor(b, g, r);
}

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
};

bool Image::parseBmp(std::vector<uint8_t>& buff) {
  std::cout << "Found BMP\n";
  int fileSize = 0;
  std::memcpy(&fileSize, &buff[2], 4);
  std::cout << "FileSize: " << fileSize << "\n";

  int width = 0;
  std::memcpy(&width, &buff[18], 4);
  std::cout << "Width: " << width << "\n";

  int height = 0;
  std::memcpy(&height, &buff[22], 4);
  std::cout << "Height: " << height << "\n";

  int dataOffset = 0;
  std::memcpy(&dataOffset, &buff[10], 4);
  std::cout << "DataOffset: " << dataOffset << "\n";

  int bitsPerPixel = 0;
  std::memcpy(&bitsPerPixel, &buff[28], 2);
  std::cout << "BitsPerPixel: " << bitsPerPixel << "\n";

  size_t colorsUsed = 0;
  std::memcpy(&colorsUsed, &buff[46], 4);
  std::cout << "ColorsUsed: " << colorsUsed << "\n";

  int compression = 0;
  std::memcpy(&compression, &buff[30], 4);
  std::cout << "Compression: " << compression << "\n";

  int tableStart = 0x36;
  int tableSz = (dataOffset - tableStart);
  std::cout << "TableSize: " << tableSz << "\n";

  m_width = width;
  m_height = height;

  m_pixels.reserve(m_width * m_height * 4);
  int paddedRowSz = ((width * bitsPerPixel + 31) / 32) * 4;
  std::vector<BGRColor> palette;

  if (bitsPerPixel <= 8) {
    for (int i = tableStart; i < tableStart + tableSz; i += 4) {
      uint8_t b = buff[i];
      uint8_t g = buff[i + 1];
      uint8_t r = buff[i + 2];

      palette.emplace_back(b, g, r);
    }
  }

  switch (bitsPerPixel) {
    case 32:
    case 24: {
      if (compression == 0) {
        for (int y = 0; y < height; ++y) {
          int fileIndex = dataOffset + ((height - 1 - y) * paddedRowSz);
          for (int x = 0; x < width; ++x) {
            int pixelIndex = fileIndex + (x * (bitsPerPixel) / 8);

            uint8_t b = buff[pixelIndex++];
            uint8_t g = buff[pixelIndex++];
            uint8_t r = buff[pixelIndex++];

            pushBgrToVec(m_pixels, BGRColor(b, g, r));
          }
        }
      }

      break;
    }

    case 16: {
      if (compression == 0) {
        for (int y = 0; y < height; ++y) {
          int fileIndex = dataOffset + ((height - 1 - y) * paddedRowSz);
          for (int x = 0; x < width; ++x) {
            int pixelIndex = fileIndex + (x * 2);

            uint16_t curr = (uint16_t)buff[pixelIndex] |
                            ((uint16_t)buff[pixelIndex + 1] << 8);
            BGRColor bgr = bgrFrom16bits(curr);

            pushBgrToVec(m_pixels, bgr);
          }
        }
      }
      break;
    }

    case 8: {
      if (palette.size() >= 256) {
        std::cerr << "Palette too large...\n";
        return false;
      }
      if (compression == 0) {
        for (int y = 0; y < height; ++y) {
          int fileIndex = dataOffset + ((height - 1 - y) * paddedRowSz);
          for (int x = 0; x < width; ++x) {
            int pixelIndex = fileIndex + x;

            if (buff[pixelIndex] >= palette.size()) {
              std::cerr << "Out of bounds color used...\n";
              return false;
            }
            BGRColor bgr = palette[buff[pixelIndex]];

            pushBgrToVec(m_pixels, bgr);
          }
        }
      }
      break;
    }

    case 4: {
      if (palette.size() > 16) {
        std::cerr << "Palette too large...\n";
        return false;
      }
      if (compression == 0) {
        for (int y = 0; y < height; ++y) {
          int fileIndex = dataOffset + ((height - 1 - y) * paddedRowSz);
          for (int x = 0; x < width; x += 2) {
            int pixelIndex = fileIndex + (x / 2);
            uint8_t byte = buff[pixelIndex];

            int idx1 = (byte >> 4) & ((1 << 4) - 1);
            if (idx1 < palette.size()) {
              pushBgrToVec(m_pixels, BGRColor(palette[idx1]));
            } else {
              std::cerr << "Out of bounds color used...\n";
              return false;
            }

            int idx2 = (byte) & ((1 << 4) - 1);
            if (x + 1 < width) {
              if (idx2 < palette.size()) {
                pushBgrToVec(m_pixels, BGRColor(palette[idx2]));
              } else {
                std::cerr << "Out of bounds color used...\n";
                return false;
              }
            }
          }
        }
      }

      break;
    }

    default: {
      std::cerr << "Not Supported yet...\n";
      return false;
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

int main() {
  Image img;
  if (!img.load("/home/aayushad/Wayland/bmpsuite-2.8/g/pal4.bmp")) {
    return 1;
  }

  render(img.getData(), img.getWidth(), img.getHeight());

  return 0;
}