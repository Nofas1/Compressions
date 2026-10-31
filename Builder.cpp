#include "Builder.h"

#include "Deflate.h"
#include "LZ77.h"

#include <stdexcept>

Builder::Builder(my_types::ChooseYourCompression option)
{
    switch (option) {
    case my_types::LZ77:
        compression = std::make_unique<LZ77>();
        break;
    case my_types::Deflate:
        compression = std::make_unique<Deflate>();
        break;
    case my_types::LZMA:
        throw std::invalid_argument("LZMA is not implemented; choose LZ77 or Deflate.");
    default:
        throw std::invalid_argument("Unknown compression algorithm.");
    }
}

Compressor* Builder::get() const noexcept
{
    return compression.get();
}
