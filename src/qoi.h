#pragma once
#include <cstdint>
#include <vector>
#include "image.h"
#include "image_decoder.h"

class QoiDecoder final : public ImageDecoder {
public:
  bool canDecode(const std::vector<uint8_t> &buff) const override;
  bool decode(const std::vector<uint8_t> &buff, Image &out) const override;

private:
  struct QoiHeader {
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint8_t colorSpace;
  };

  QoiHeader parseHeader(const std::vector<uint8_t> &buff) const;
};
