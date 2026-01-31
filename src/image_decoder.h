#pragma once
#include <cstdint>
#include <vector>

class Image;

class ImageDecoder {
public:
  virtual ~ImageDecoder() = default;
  virtual bool canDecode(const std::vector<uint8_t> &buff) const = 0;
  virtual bool decode(const std::vector<uint8_t> &buff, Image &out) const = 0;
};
