#pragma once

#include "Compressor.h"
#include "My_types.h"

#include <memory>

class Builder
{
    std::unique_ptr<Compressor> compression;

public:
    explicit Builder(my_types::ChooseYourCompression option);

    Compressor* get() const noexcept;
};
