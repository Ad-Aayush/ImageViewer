#include "image.h"
#include <iostream>
#include <optional>

void Image::setDimensions(int width, int height) {
  m_width = width;
  m_height = height;
  m_pixels.clear();
}

void Image::reservePixels(size_t bytes) { m_pixels.reserve(bytes); }

void Image::pushPixel(const RGBA &color) { pushRgbaToVec(m_pixels, color); }

void Image::writePixelAtByteIndex(int32_t idx, const RGBA &color) {
  writeBgrToIdx(m_pixels, color, idx);
}

void Image::fill(const RGBA &color) {
  size_t totalBytes = static_cast<size_t>(m_width) * m_height * 4;
  m_pixels.resize(totalBytes);
  for (size_t i = 0; i < totalBytes; i += 4) {
    m_pixels[i] = color.b;
    m_pixels[i + 1] = color.g;
    m_pixels[i + 2] = color.r;
    m_pixels[i + 3] = color.a;
  }
}

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
