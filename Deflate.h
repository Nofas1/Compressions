#pragma once

#include "Compressor.h"

class Deflate final : public Compressor
{
public:
    void compress() override;
    void decompress() override;
};
