#pragma once
#include "image.h"
#include "image_decoder.h"
#include <cstdint>
#include <vector>

const std::vector<uint8_t> PNG_SIG = {0x89, 0x50, 0x4E, 0x47,
                                      0x0D, 0x0A, 0x1A, 0x0A};
const std::vector<uint32_t> WEIRD_ORDER = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                           11, 4,  12, 3, 13, 2, 14, 1, 15};

class PngDecoder final : public ImageDecoder {
public:
  bool canDecode(const std::vector<uint8_t> &buff) const override;
  bool decode(const std::vector<uint8_t> &buff, Image &out) const override;

  struct Ihdr {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compMethod;
    uint8_t filterMethod;
    uint8_t interlceMethod;
    // Derieved data
    uint8_t channels;
    uint8_t bitsPerPixel;
  };

private:
  bool parsePng(const std::vector<uint8_t> &buff, Image &out) const;
  Ihdr parseIhdr(const std::vector<uint8_t> &buff, int &idx) const;
};
