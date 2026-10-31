#pragma once

#include "Builder.h"
#include "My_types.h"

#include <string>

class Archivator
{
    Builder build;
    Compressor* compression;

public:
    explicit Archivator(my_types::ChooseYourCompression option);

    void send_buffer(const std::string& buffer);
    void save(const std::string& output_name) const;
    void user_compress(const std::string& output_name);
    void user_decompress(const std::string& input_name, const std::string& output_name);
};
