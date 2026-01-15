#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>


extern "C" {
void render(const uint8_t *buffer, int width, int height);
}

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

  bool parseBmp(std::vector<uint8_t> &buff);
};

bool Image::parseBmp(std::vector<uint8_t> &buff) {
  std::cout << "Found BMP\n";
  int fileSize;
  std::memcpy(&fileSize, &buff[2], 4);
  std::cout << "FileSize: " << fileSize << "\n";

  int width;
  std::memcpy(&width, &buff[18], 4);
  std::cout << "Width: " << width << "\n";

  int height;
  std::memcpy(&height, &buff[22], 4);
  std::cout << "Height: " << height << "\n";

  int dataOffset;
  std::memcpy(&dataOffset, &buff[10], 4);
  std::cout << "DataOffset: " << dataOffset << "\n";

  int bitsPerPixel = 0;
  std::memcpy(&bitsPerPixel, &buff[28], 2);
  std::cout << "BitsPerPixel: " << bitsPerPixel << "\n";

  int colorsUsed;
  std::memcpy(&colorsUsed, &buff[46], 4);
  std::cout << "ColorsUsed: " << colorsUsed << "\n";

  int compression;
  std::memcpy(&compression, &buff[30], 4);
  std::cout << "Compression: " << compression << "\n";

  m_width = width;
  m_height = height;

  m_pixels.reserve(m_width * m_height * 4);
  switch (bitsPerPixel) {
  case 24: {
    if (compression == 0) {
      int paddedRowSz = ((width * 3 + 3) / 4) * 4;

      for (int y = 0; y < height; ++y) {
        int fileIndex = dataOffset + ((height - 1 - y) * paddedRowSz);
        for (int x = 0; x < width; ++x) {
          int pixelIndex = fileIndex + (x * 3);
          
          uint8_t b = buff[pixelIndex++];
          uint8_t g = buff[pixelIndex++];
          uint8_t r = buff[pixelIndex++];

          m_pixels.push_back(b);
          m_pixels.push_back(g);
          m_pixels.push_back(r);
          m_pixels.push_back(255);
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

  if (byteBuf.size() >= 2 && byteBuf[0] == 'B' && byteBuf[1] == 'M') {
    return parseBmp(byteBuf);
  }

  return false;
}

int main() {
  Image img;
  img.load("image.bmp");

  render(img.getData(), img.getWidth(), img.getHeight());

  return 0;
}