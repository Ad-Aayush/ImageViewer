#include <cstdint>
#include <iostream>
#include <string>
#include "image.h"
#include "image_loader.h"

extern "C" {
void render(const uint8_t *buffer, int width, int height);
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <image-file>\n";
    return 1;
  }
  std::string filePath = argv[1];

  Image img;
  ImageLoader loader;
  if (!loader.loadFromFile(filePath, img)) {
    return 1;
  }

  render(img.data(), img.width(), img.height());

  return 0;
}
