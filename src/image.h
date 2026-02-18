#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct RGBA {
  uint8_t b, g, r, a;

  RGBA() = default;
  RGBA(uint8_t b, uint8_t g, uint8_t r, uint8_t a = 255)
      : b(b), g(g), r(r), a(a) {}
};

std::optional<uint32_t> readNextByte(const std::vector<uint8_t> &buff,
                                     int &buffIdx);
std::optional<uint32_t>
read4BytesAsU32(const std::vector<uint8_t> &buff, int &idx);

std::optional<std::string>
read4BytesAsStr(const std::vector<uint8_t> &buff, int &idx);

std::optional<uint32_t> readNextNbits(const std::vector<uint8_t> &buff, int N,
                                      int &byteIdx, int &bitIdx);


void pushRgbaToVec(std::vector<uint8_t> &pixels, const RGBA color);
void writeBgrToIdx(std::vector<uint8_t> &pixels, const RGBA color, int32_t idx);
RGBA bgrFrom16bits(uint16_t curr);

class Image {
public:
  Image() = default;

  int height() const { return m_height; }
  int width() const { return m_width; }
  const uint8_t *data() const { return m_pixels.data(); }

  void setDimensions(int width, int height);
  void reservePixels(size_t bytes);
  void pushPixel(const RGBA &color);
  void writePixelAtByteIndex(int32_t idx, const RGBA &color);
  void fill(const RGBA &color);

private:
  int m_height = 0;
  int m_width = 0;
  std::vector<uint8_t> m_pixels;
};
