#pragma once

#include "Compressor.h"

class LZ77 final : public Compressor
{
public:
    void compress() override;
    void decompress() override;
};
