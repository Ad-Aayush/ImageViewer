#pragma once
#include <optional>
#include <string>
#include <vector>
#include "image.h"
#include "image_decoder.h"

class PngDecoder final : public ImageDecoder {
public:
  bool canDecode(const std::vector<uint8_t> &buff) const override;
  bool decode(const std::vector<uint8_t> &buff, Image &out) const override;

private:
  bool parsePng(const std::vector<uint8_t> &buff) const;
  std::optional<uint32_t> read4BytesAsU32(const std::vector<uint8_t> &buff,
                                          int &idx) const;
  std::optional<std::string> read4BytesAsStr(const std::vector<uint8_t> &buff,
                                             int &idx) const;
};
