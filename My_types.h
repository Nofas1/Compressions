#pragma once

#include <utility>

namespace my_types {

constexpr int BUFFER_SIZE = 258;
constexpr int size_of_window = 32768;

using ll = long long;
using ld = long double;
using pii = std::pair<int, int>;
using pll = std::pair<ll, ll>;
using pic = std::pair<int, char>;

enum ChooseYourCompression {
    LZ77 = 1,
    LZMA = 2,
    Deflate = 3
};

enum CompressOrDecompress {
    compress = 1,
    decompress
};

} // namespace my_types
