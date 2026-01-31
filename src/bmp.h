#pragma once
#include <cstdint>
#include <vector>
#include "image.h"
#include "image_decoder.h"

class BmpDecoder final : public ImageDecoder {
public:
  bool canDecode(const std::vector<uint8_t> &buff) const override;
  bool decode(const std::vector<uint8_t> &buff, Image &out) const override;

private:
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

  BmpHeader parseHeader(const std::vector<uint8_t> &buff) const;
  bool decode24Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                   Image &out) const;
  bool decode16Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                   Image &out) const;
  bool decode8Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette, Image &out) const;
  bool decodeRle8(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette, Image &out) const;
  bool decode4Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette, Image &out) const;
  bool decodeRle4(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette, Image &out) const;
  bool decode1Bit(const std::vector<uint8_t> &buff, const BmpHeader &header,
                  const std::vector<RGBA> &palette, Image &out) const;
};
