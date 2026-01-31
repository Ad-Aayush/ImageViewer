#pragma once
#include "image.h"
#include "image_decoder.h"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class PngDecoder final : public ImageDecoder {
public:
  bool canDecode(const std::vector<uint8_t> &buff) const override;
  bool decode(const std::vector<uint8_t> &buff, Image &out) const override;

private:
  struct Ihdr {
    uint32_t width;
    uint32_t height;
    uint8_t bitDepth;
    uint8_t colorType;
    uint8_t compMethod;
    uint8_t filterMethod;
    uint8_t interlceMethod;
  };
  bool parsePng(const std::vector<uint8_t> &buff, Image &out) const;
  Ihdr parseIhdr(const std::vector<uint8_t> &buff, int &idx) const;
  std::optional<uint32_t> read4BytesAsU32(const std::vector<uint8_t> &buff,
                                          int &idx) const;
  std::optional<std::string> read4BytesAsStr(const std::vector<uint8_t> &buff,
                                             int &idx) const;
};
