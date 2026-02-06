#include "png_decoder.h"
#include "image.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <sys/types.h>
#include <vector>

struct Trie {
  std::vector<int> left, right, sym;
  uint sz;

  Trie() {
    sz = 1;
    left.push_back(-1);
    right.push_back(-1);
    sym.push_back(-1);
  }

  bool isSym(uint node) {
    if (sym[node] != -1) {
      return true;
    }
    return false;
  }

  int getSym(uint node) { return sym[node]; }

  uint goLeft(uint node) {
    if (left[node] != -1) {
      return left[node];
    }
    return insertLeft(node);
  }
  uint goRight(uint node) {
    if (right[node] != -1) {
      return right[node];
    }
    return insertRight(node);
  }

  uint insertLeft(uint node) {
    if (node >= sz) {
      return 0;
    }
    left[node] = sz;
    left.push_back(-1);
    right.push_back(-1);
    sym.push_back(-1);
    return sz++;
  }
  uint insertRight(uint node) {
    if (node >= sz) {
      return 0;
    }
    right[node] = sz;
    left.push_back(-1);
    right.push_back(-1);
    sym.push_back(-1);
    return sz++;
  }
  bool setSym(uint node, uint s) {
    if (node >= sz) {
      return false;
    }
    if (left[node] != -1 or right[node] != -1) {
      std::cout << "Not Prefix Free\n";
      return false;
    }
    sym[node] = s;
    return true;
  }
};

bool PngDecoder::canDecode(const std::vector<uint8_t> &buff) const {
  if (buff.size() < 8) {
    return false;
  }
  if (buff[0] != 0x89) {
    return false;
  }
  if (buff[1] != 0x50) {
    return false;
  }
  if (buff[2] != 0x4E) {
    return false;
  }
  if (buff[3] != 0x47) {
    return false;
  }
  if (buff[4] != 0x0D) {
    return false;
  }
  if (buff[5] != 0x0A) {
    return false;
  }
  if (buff[6] != 0x1A) {
    return false;
  }
  if (buff[7] != 0x0A) {
    return false;
  }
  return true;
}

std::optional<uint32_t>
PngDecoder::read4BytesAsU32(const std::vector<uint8_t> &buff, int &idx) {
  uint32_t ans = 0;
  if (idx + 4 > (int)buff.size()) {
    std::cerr << "Unexpected End of file...\n";
    return {};
  }

  int cnt = 0;

  while (cnt < 4) {
    ans = (ans << 8) | (uint32_t)buff[idx++];
    cnt++;
  }

  return ans;
}

std::optional<std::string>
PngDecoder::read4BytesAsStr(const std::vector<uint8_t> &buff, int &idx) const {
  std::string ans;
  if (idx + 4 > (int)buff.size()) {
    std::cerr << "Unexpected End of file...\n";
    return {};
  }

  int cnt = 0;

  while (cnt < 4) {
    ans += (char)buff[idx++];
    cnt++;
  }

  return ans;
}

std::optional<uint32_t> readNextNbits(const std::vector<uint8_t> &buff, int N,
                                      int &byteIdx, int &bitIdx) {
  if (N > 32) {
    return {};
  }
  uint32_t out = 0;
  int curr = 0;
  while (curr < N) {
    if (bitIdx == 8) {
      bitIdx = 0;
      byteIdx++;
    }
    if ((size_t)byteIdx >= buff.size()) {
      return {};
    }
    uint8_t currByte = buff[byteIdx];
    out ^= ((currByte >> bitIdx) & 1) << curr;
    curr++;
    bitIdx++;
  }
  if (bitIdx == 8) {
    bitIdx = 0;
    byteIdx++;
  }
  return out;
}

std::vector<uint16_t>
buildHuffmanFromBitLen(const std::vector<uint8_t> &bitLen) {
  std::array<uint16_t, 16> bl_count{}, next_code{};
  for (auto &x : bitLen) {
    if (x < 16) {
      bl_count[x]++;
    }
  }
  std::vector<uint16_t> huffCode(bitLen.size());
  uint16_t code = 0;
  bl_count[0] = 0;
  for (int bits = 1; bits <= 15; bits++) {
    code = (code + bl_count[bits - 1]) << 1;
    next_code[bits] = code;
  }
  for (size_t i = 0; i < bitLen.size(); ++i) {
    if (bitLen[i] != 0) {
      huffCode[i] = next_code[bitLen[i]];
      next_code[bitLen[i]]++;
    }
  }
  return huffCode;
}

Trie decoderFromBitLen(const std::vector<uint8_t> &bitLen) {
  std::vector<uint16_t> huffCodes = buildHuffmanFromBitLen(bitLen);
  Trie decoder;
  int sz = bitLen.size();
  for (int i = 0; i < sz; ++i) {
    int len = bitLen[i];
    if (len == 0) {
      continue;
    }
    uint16_t code = huffCodes[i];
    uint currNode = 0;
    for (int j = len - 1; j >= 0; --j) {
      uint8_t bit = (code >> j) & 1;
      if (bit == 0) {
        currNode = decoder.goLeft(currNode);
      } else {
        currNode = decoder.goRight(currNode);
      }
    }
    decoder.setSym(currNode, i);
  }
  return decoder;
}

std::optional<std::vector<uint8_t>> inflate(const std::vector<uint8_t> &enc) {
  int idx = 0;
  auto maybeCmf = readNextByte(enc, idx);
  auto maybeFlg = readNextByte(enc, idx);

  if (!maybeCmf.has_value() or !maybeFlg.has_value()) {
    return {};
  }
  uint8_t cmf = maybeCmf.value(), flg = maybeFlg.value();
  uint8_t compMethod = cmf & ((1 << 4) - 1);
  uint8_t cInfo = cmf >> 4;
  if (compMethod != 8) {
    return {};
  }
  if (cInfo > 7) {
    return {};
  }
  uint8_t fDict = flg & (1 << 5);
  uint8_t fLevel = flg >> 6;
  if ((256 * cmf + flg) % 31 != 0) {
    return {};
  }
  if (fDict != 0) {
    return {};
  }
  std::cout << "Compression Level: " << (uint32_t)fLevel << "\n";

  std::vector<uint8_t> litlen_extra(29);
  for (int i = 8; i < 28; i++) {
    litlen_extra[i] = (i - 4) / 4;
  }
  std::vector<uint32_t> litlen_base(29);
  uint32_t val = 3;
  for (int i = 0; i < 29; i++) {
    litlen_base[i] = val;
    val += 1 << litlen_extra[i];
  }
  litlen_base[28] = 258;
  std::vector<uint32_t> dist_extra(30);
  for (int i = 4; i < 30; ++i) {
    dist_extra[i] = (i - 2) / 2;
  }
  std::vector<uint32_t> dist_base(30);
  val = 1;
  for (int i = 0; i < 30; i++) {
    dist_base[i] = val;
    val += 1 << dist_extra[i];
  }
  std::vector<uint8_t> litlen_bitlen(288);
  for (int i = 0; i < 144; ++i) {
    litlen_bitlen[i] = 8;
  }
  for (int i = 144; i < 256; ++i) {
    litlen_bitlen[i] = 9;
  }
  for (int i = 256; i < 280; ++i) {
    litlen_bitlen[i] = 7;
  }
  for (int i = 280; i < 288; ++i) {
    litlen_bitlen[i] = 8;
  }
  std::vector<uint8_t> dist_bitlen(32, 5);
  Trie fLitlenTrie = decoderFromBitLen(litlen_bitlen);
  Trie fDistTrie = decoderFromBitLen(dist_bitlen);

  std::vector<uint8_t> data;
  int bitIdx = 0;
  bool final = false;
  while (!final) {
    auto maybeFinal = readNextNbits(enc, 1, idx, bitIdx);
    if (!maybeFinal.has_value()) {
      return {};
    }
    final = maybeFinal.value();
    auto maybeType = readNextNbits(enc, 2, idx, bitIdx);
    if (!maybeType.has_value()) {
      return {};
    }
    uint8_t type = maybeType.value();

    switch (type) {
    case 0b00: {
      if (bitIdx != 0) {
        bitIdx = 0;
        idx++;
      }
      auto maybeLow = readNextByte(enc, idx),
           maybeHigh = readNextByte(enc, idx);
      if (!maybeLow.has_value() or !maybeHigh.has_value()) {
        return {};
      }
      uint16_t low = maybeLow.value(), high = maybeHigh.value();
      uint16_t len = low + (high << 8);
      auto maybeLowN = readNextByte(enc, idx),
           maybeHighN = readNextByte(enc, idx);
      if (!maybeLowN.has_value() or !maybeHighN.has_value()) {
        return {};
      }
      uint16_t lowN = maybeLowN.value(), highN = maybeHighN.value();
      uint16_t lenN = lowN + (highN << 8);
      if ((len ^ lenN) != 0xFFFF) {
        return {};
      }
      for (int i = 0; i < len; ++i) {
        auto maybeNextByte = readNextByte(enc, idx);
        if (!maybeNextByte.has_value()) {
          return {};
        }
        data.push_back(maybeNextByte.value());
      }
      break;
    }

    case 0b01: {
      while (true) {
        int trieIdx = 0;
        // Decode the huffman code.
        while (!fLitlenTrie.isSym(trieIdx)) {
          auto maybeNextBit = readNextNbits(enc, 1, idx, bitIdx);
          if (!maybeNextBit.has_value()) {
            return {};
          }
          uint8_t nextBit = maybeNextBit.value();
          if (nextBit == 0) {
            trieIdx = fLitlenTrie.left[trieIdx];
          } else {
            trieIdx = fLitlenTrie.right[trieIdx];
          }
          if (trieIdx == -1) {
            std::cout << "Issue decoding huffman code...\n";
            return {};
          }
        }
        int sym = fLitlenTrie.getSym(trieIdx);

        if (sym == 256) {
          break;
        } else if (sym < 256) {
          data.push_back((uint8_t)sym);
        } else if (sym > 285) {
          return {};
        } else {
          uint32_t baseLen = litlen_base[sym - 257];
          uint8_t extraBits = litlen_extra[sym - 257];
          uint32_t extraLen = 0;
          for (int i = 0; i < extraBits; ++i) {
            auto maybeBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeBit.has_value()) {
              return {};
            }
            uint8_t bit = maybeBit.value();
            extraLen = (bit << i) | extraLen;
          }
          uint32_t len = baseLen + extraLen;

          trieIdx = 0;
          while (!fDistTrie.isSym(trieIdx)) {
            auto maybeNextBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeNextBit.has_value()) {
              return {};
            }
            uint8_t nextBit = maybeNextBit.value();
            if (nextBit == 0) {
              trieIdx = fDistTrie.left[trieIdx];
            } else {
              trieIdx = fDistTrie.right[trieIdx];
            }
            if (trieIdx == -1) {
              std::cout << "Issue decoding huffman code...\n";
              return {};
            }
          }
          sym = fDistTrie.getSym(trieIdx);
          if (sym > 29) {
            return {};
          }

          uint32_t baseDist = dist_base[sym];
          extraBits = dist_extra[sym];
          uint32_t extraDist = 0;
          for (int i = 0; i < extraBits; ++i) {
            auto maybeBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeBit.has_value()) {
              return {};
            }
            uint8_t bit = maybeBit.value();
            extraDist = (bit << i) | extraDist;
          }
          uint32_t dist = baseDist + extraDist;
          uint32_t size = data.size();
          if (size < dist) {
            return {};
          }
          for (uint32_t i = 0; i < len; i++) {
            size = data.size();
            data.push_back(data[size - dist]);
          }
        }
      }
      break;
    }

    case 0b10: {
      auto maybeHlit = readNextNbits(enc, 5, idx, bitIdx),
           maybeHdist = readNextNbits(enc, 5, idx, bitIdx),
           maybeHclen = readNextNbits(enc, 4, idx, bitIdx);
      if (!maybeHlit.has_value() or !maybeHdist.has_value() or
          !maybeHclen.has_value()) {
        return {};
      }
      uint32_t hlit = maybeHlit.value() + 257, hdist = maybeHdist.value() + 1,
               hclen = maybeHclen.value() + 4;
      // std::cout << "HLIT: " << hlit << ", HDIST: " << hdist
                // << ", HCLEN: " << hclen << "\n";
      std::vector<uint32_t> weirdOrder = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                          11, 4,  12, 3, 13, 2, 14, 1, 15};
      std::vector<uint8_t> clen_len(19);
      for (uint8_t i = 0; i < hclen; ++i) {
        auto maybeEntry = readNextNbits(enc, 3, idx, bitIdx);
        if (!maybeEntry.has_value()) {
          return {};
        }
        uint8_t entry = maybeEntry.value();
        clen_len[weirdOrder[i]] = entry;
      }
      Trie metaTrie = decoderFromBitLen(clen_len);
      uint16_t totalCodes = hlit + hdist;
      uint16_t currCode = 0;
      std::vector<uint8_t> code_lens;

      while (currCode < totalCodes) {
        int trieIdx = 0;
        while (!metaTrie.isSym(trieIdx)) {
          auto maybeNextBit = readNextNbits(enc, 1, idx, bitIdx);
          if (!maybeNextBit.has_value()) {
            return {};
          }
          uint8_t nextBit = maybeNextBit.value();
          if (nextBit == 0) {
            trieIdx = metaTrie.left[trieIdx];
          } else {
            trieIdx = metaTrie.right[trieIdx];
          }
          if (trieIdx == -1) {
            std::cout << "Issue decoding huffman code...\n";
            return {};
          }
        }
        int code = metaTrie.getSym(trieIdx);

        if (code <= 15) {
          code_lens.push_back(code);
          currCode++;
        } else if (code == 16) {
          auto maybeFreq = readNextNbits(enc, 2, idx, bitIdx);
          if (!maybeFreq.has_value()) {
            return {};
          }
          uint8_t freq = maybeFreq.value() + 3;
          if (code_lens.size() != 0 and currCode + freq <= totalCodes) {
            for (uint8_t i = 0; i < freq; ++i) {
              code_lens.push_back(code_lens.back());
              currCode++;
            }
          } else {
            return {};
          }
        } else if (code == 17) {
          auto maybeFreq = readNextNbits(enc, 3, idx, bitIdx);
          if (!maybeFreq.has_value()) {
            return {};
          }
          uint8_t freq = maybeFreq.value() + 3;
          if (currCode + freq <= totalCodes) {
            for (uint8_t i = 0; i < freq; ++i) {
              code_lens.push_back(0);
              currCode++;
            }
          } else {
            return {};
          }
        } else if (code == 18) {
          auto maybeFreq = readNextNbits(enc, 7, idx, bitIdx);
          if (!maybeFreq.has_value()) {
            return {};
          }
          uint8_t freq = maybeFreq.value() + 11;
          if (currCode + freq <= totalCodes) {
            for (uint8_t i = 0; i < freq; ++i) {
              code_lens.push_back(0);
              currCode++;
            }
          } else {
            return {};
          }
        }
      }
      std::vector<uint8_t> lit_lens(hlit), dist_lens(hdist);
      bool allDistZero = true;

      for (uint32_t i = 0; i < hlit; i++) {
        lit_lens[i] = code_lens[i];
      }
      for (int i = hlit; i < totalCodes; ++i) {
        dist_lens[i - hlit] = code_lens[i];
        if (code_lens[i] != 0) {
          allDistZero = false;
        }
      }

      if (lit_lens[256] == 0) {
        return {};
      }

      Trie litTrie = decoderFromBitLen(lit_lens),
           distTrie = decoderFromBitLen(dist_lens);

      while (true) {
        int trieIdx = 0;
        while (!litTrie.isSym(trieIdx)) {
          auto maybeNextBit = readNextNbits(enc, 1, idx, bitIdx);
          if (!maybeNextBit.has_value()) {
            return {};
          }
          uint8_t nextBit = maybeNextBit.value();
          if (nextBit == 0) {
            trieIdx = litTrie.left[trieIdx];
          } else {
            trieIdx = litTrie.right[trieIdx];
          }
          if (trieIdx == -1) {
            std::cout << "Issue decoding huffman code...\n";
            return {};
          }
        }
        int sym = litTrie.getSym(trieIdx);

        if (sym == 256) {
          break;
        } else if (sym < 256) {
          data.push_back((uint8_t)sym);
        } else if (sym > 285) {
          return {};
        } else if (!allDistZero) {
          uint32_t baseLen = litlen_base[sym - 257];
          uint8_t extraBits = litlen_extra[sym - 257];
          uint32_t extraLen = 0;
          for (int i = 0; i < extraBits; ++i) {
            auto maybeBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeBit.has_value()) {
              return {};
            }
            uint8_t bit = maybeBit.value();
            extraLen = (bit << i) | extraLen;
          }
          uint32_t len = baseLen + extraLen;

          trieIdx = 0;
          while (!distTrie.isSym(trieIdx)) {
            auto maybeNextBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeNextBit.has_value()) {
              return {};
            }
            uint8_t nextBit = maybeNextBit.value();
            if (nextBit == 0) {
              trieIdx = distTrie.left[trieIdx];
            } else {
              trieIdx = distTrie.right[trieIdx];
            }
            if (trieIdx == -1) {
              std::cout << "Issue decoding huffman code...\n";
              return {};
            }
          }
          sym = distTrie.getSym(trieIdx);

          uint32_t baseDist = dist_base[sym];
          extraBits = dist_extra[sym];
          uint32_t extraDist = 0;
          for (int i = 0; i < extraBits; ++i) {
            auto maybeBit = readNextNbits(enc, 1, idx, bitIdx);
            if (!maybeBit.has_value()) {
              return {};
            }
            uint8_t bit = maybeBit.value();
            extraDist = (bit << i) | extraDist;
          }
          uint32_t dist = baseDist + extraDist;
          uint32_t size = data.size();
          if (size < dist) {
            return {};
          }
          for (uint32_t i = 0; i < len; i++) {
            size = data.size();
            data.push_back(data[size - dist]);
          }
        } else {
          return {};
        }
      }
      break;
    }
    default: {
      break;
    }
    }
  }
  if (bitIdx != 0) {
    bitIdx = 0;
    idx++;
  }
  // Adler-32 match
  uint32_t s1 = 1, s2 = 0;
  for (int i = 0; i < (int)data.size(); i++) {
    s1 = (s1 + (uint32_t)data[i]) % 65521;
    s2 = (s2 + s1) % 65521;
  }
  uint32_t exp = (s2 << 16) | s1;
  auto maybeAdl = PngDecoder::read4BytesAsU32(enc, idx);
  if (!maybeAdl.has_value()) {
    return {};
  }
  uint32_t adl = maybeAdl.value();
  if (adl != exp) {
    std::cout << "Checksum failed\n";
    std::cout << "Expected: " << exp << "\n";
    std::cout << "Found: " << adl << "\n";
    return {};
  }

  std::cout << "Successfully Inflated\n";
  return data;
}

PngDecoder::Ihdr PngDecoder::parseIhdr(const std::vector<uint8_t> &buff,
                                       int &idx) const {
  Ihdr header{};
  for (int i = idx; i < idx + 4; ++i) {
    header.width = (header.width << 8) | (uint32_t)buff[i];
  }
  std::cout << "Width: " << header.width << "\n";
  idx += 4;

  for (int i = idx; i < idx + 4; ++i) {
    header.height = (header.height << 8) | (uint32_t)buff[i];
  }
  std::cout << "Height: " << header.height << "\n";
  idx += 4;

  std::memcpy(&header.bitDepth, &buff[idx], 1);
  std::cout << "BitDepth: " << (uint32_t)header.bitDepth << "\n";
  idx += 1;

  std::memcpy(&header.colorType, &buff[idx], 1);
  std::cout << "ColorType: " << (uint32_t)header.colorType << "\n";
  idx += 1;

  std::memcpy(&header.compMethod, &buff[idx], 1);
  std::cout << "CompressionMethod: " << (uint32_t)header.compMethod << "\n";
  idx += 1;

  std::memcpy(&header.filterMethod, &buff[idx], 1);
  std::cout << "FilterMethod: " << (uint32_t)header.filterMethod << "\n";
  idx += 1;

  std::memcpy(&header.interlceMethod, &buff[idx], 1);
  std::cout << "InterlaceMethod: " << (uint32_t)header.interlceMethod << "\n";
  idx += 1;

  return header;
}

int paethPredictor(int a, int b, int c) {
  int p = a + b - c;
  int da = std::abs(p - a);
  int db = std::abs(p - b);
  int dc = std::abs(p - c);

  if (da <= db and da <= dc) {
    return a;
  } else if (db <= da and db <= dc) {
    return b;
  } else {
    return c;
  }
}

std::optional<std::vector<uint8_t>> unfilter(const std::vector<uint8_t> &buff,
                                             const PngDecoder::Ihdr &header) {
  int rowBytes = ((header.width * header.bitsPerPixel + 7) / 8);
  int bpp = (header.bitsPerPixel + 7) / 8;
  std::vector<uint8_t> unfiltered;
  std::vector<uint8_t> prevRowRecon(rowBytes, 0);
  int idx = 0;
  for (uint32_t i = 0; i < header.height; ++i) {
    auto maybeFilterType = readNextByte(buff, idx);
    if (!maybeFilterType.has_value()) {
      return {};
    }
    int filterType = maybeFilterType.value();
    std::vector<uint8_t> currRow;
    currRow.reserve(rowBytes);
    std::vector<uint8_t> recon(rowBytes, 0);
    for (int j = 0; j < rowBytes; ++j) {
      auto maybeNextByte = readNextByte(buff, idx);
      if (!maybeNextByte.has_value()) {
        return {};
      }
      currRow.push_back(maybeNextByte.value());
    }

    for (int j = 0; j < rowBytes; ++j) {
      uint8_t a, b, c, x;
      x = currRow[j];
      if (j >= bpp) {
        a = recon[j - bpp];
        c = prevRowRecon[j - bpp];
      } else {
        a = 0;
        c = 0;
      }
      b = prevRowRecon[j];

      switch (filterType) {
      case 0:
        recon[j] = x;
        break;

      case 1:
        recon[j] = x + a;
        break;
      case 2:
        recon[j] = x + b;
        break;
      case 3:
        recon[j] = x + (a + b) / 2;
        break;

      case 4:
        recon[j] = x + paethPredictor(a, b, c);
        break;

      default:
        std::cerr << "Unknown Filter Type...\n";
        return {};
      }
    }

    for (int j = 0; j < rowBytes; ++j) {
      prevRowRecon[j] = recon[j];
      unfiltered.push_back(recon[j]);
    }
  }

  return unfiltered;
}

bool PngDecoder::parsePng(const std::vector<uint8_t> &buff, Image &out) const {
  std::cout << "Found Png...\n";
  int idx = 8;
  Ihdr header;
  std::vector<uint8_t> encData;

  while (true) {
    std::cout << "\n";
    auto maybeLen = read4BytesAsU32(buff, idx);

    if (!maybeLen.has_value()) {
      return false;
    }

    uint32_t len = maybeLen.value();
    std::cout << "Length: " << len << "\n";

    auto maybeChunkType = read4BytesAsStr(buff, idx);

    if (!maybeChunkType.has_value()) {
      return false;
    }

    std::string chunkType = maybeChunkType.value();
    std::cout << "ChunkType: " << chunkType << "\n";

    if (chunkType == "IHDR") {
      header = parseIhdr(buff, idx);
      out.setDimensions(header.width, header.height);
      out.reservePixels(static_cast<size_t>(header.width) * header.height * 4);

    } else if (chunkType == "IDAT") {
      encData.reserve(encData.size() + len);
      for (uint32_t i = 0; i < len; ++i) {
        encData.push_back(buff[idx++]);
      }
    } else {
      idx += len;
    }

    auto maybeCrc = read4BytesAsU32(buff, idx);

    if (!maybeCrc.has_value()) {
      return false;
    }

    uint32_t crc = maybeCrc.value();
    std::cout << "Crc: " << crc << "\n";

    if (chunkType == "IEND") {
      break;
    }
  }
  auto maybeInflated = inflate(encData);
  if (!maybeInflated.has_value()) {
    return false;
  }
  std::vector<uint8_t> inflated = maybeInflated.value();
  // Unfilter
  switch (header.colorType) {
  case 0:
    header.channels = 1;
    break;

  case 2:
    header.channels = 3;
    break;

  case 3:
    header.channels = 1;
    break;

  case 4:
    header.channels = 2;
    break;

  case 6:
    header.channels = 4;
    break;

  default:
    std::cerr << "Unknown color type...\n";
    return {};
  }
  header.bitsPerPixel = header.channels * header.bitDepth;
  uint16_t filterBpp = std::ceil(header.bitsPerPixel / 8.0);
  std::cout << "Filter BPP: " << filterBpp << "\n";
  auto maybeUnfiltered = unfilter(inflated, header);
  if (!maybeUnfiltered.has_value()) {
    return false;
  }
  std::vector<uint8_t> unfiltered = maybeUnfiltered.value();
  int p = 0;
  for (uint32_t x = 0; x < header.height; ++x) {
    for (uint32_t y = 0; y < header.width; ++y) {
      if (header.channels == 3) {
        auto maybeR = readNextByte(unfiltered, p),
             maybeG = readNextByte(unfiltered, p),
             maybeB = readNextByte(unfiltered, p);
        if (!maybeR.has_value() or !maybeG.has_value() or !maybeB.has_value()) {
          return false;
        }
        uint8_t r = maybeR.value(), g = maybeG.value(), b = maybeB.value();
        int writeIdx = 4 * (x * header.width + y);
        out.writePixelAtByteIndex(writeIdx, RGBA(b, g, r));
      } else if (header.channels == 4) {
        auto maybeR = readNextByte(unfiltered, p),
             maybeG = readNextByte(unfiltered, p),
             maybeB = readNextByte(unfiltered, p),
             maybeA = readNextByte(unfiltered, p);
        if (!maybeR.has_value() or !maybeG.has_value() or !maybeB.has_value() or
            !maybeA.has_value()) {
          return false;
        }
        uint8_t r = maybeR.value(), g = maybeG.value(), b = maybeB.value(), a = maybeA.value();
        int writeIdx = 4 * (x * header.width + y);
        out.writePixelAtByteIndex(writeIdx, RGBA(b, g, r, a));
      }
    }
  }

  return true;
}

bool PngDecoder::decode(const std::vector<uint8_t> &buff, Image &out) const {
  out.setDimensions(0, 0);
  out.reservePixels(0);
  return parsePng(buff, out);
}
