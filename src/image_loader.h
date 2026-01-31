#pragma once
#include <memory>
#include <string>
#include <vector>
#include "image_decoder.h"
#include "image.h"

class ImageLoader {
public:
  ImageLoader();
  bool loadFromFile(const std::string &file, Image &out) const;

private:
  std::vector<std::unique_ptr<ImageDecoder>> m_decoders;
};
