#include "image_loader.h"
#include <fstream>
#include <iostream>
#include <vector>
#include "bmp.h"
#include "image.h"
#include "image_decoder.h"
#include "png_decoder.h"
#include "qoi.h"

ImageLoader::ImageLoader() {
  m_decoders.emplace_back(std::make_unique<BmpDecoder>());
  m_decoders.emplace_back(std::make_unique<QoiDecoder>());
  m_decoders.emplace_back(std::make_unique<PngDecoder>());
}

bool ImageLoader::loadFromFile(const std::string &file, Image &out) const {
  std::ifstream inp(file, std::ios::binary | std::ios::ate);
  if (!inp) {
    std::cerr << "Failed to open " << file << "\n";
    return false;
  }

  std::streamsize size = inp.tellg();
  if (size <= 0) {
    return false;
  }

  inp.seekg(0, std::ios::beg);

  std::vector<uint8_t> byteBuf(static_cast<size_t>(size));
  if (!inp.read(reinterpret_cast<char *>(byteBuf.data()), size)) {
    return false;
  }

  for (const auto &decoder : m_decoders) {
    if (!decoder->canDecode(byteBuf)) {
      continue;
    }
    return decoder->decode(byteBuf, out);
  }

  return false;
}
