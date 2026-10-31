#include "Archivator.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

Archivator::Archivator(my_types::ChooseYourCompression option)
    : build(option), compression(build.get())
{
}

void Archivator::send_buffer(const std::string& buffer)
{
    compression->load(buffer);
}

void Archivator::save(const std::string& output_name) const
{
    std::ofstream output(output_name, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot open output file: " + output_name);
    }

    const std::string& contents = compression->get_result();
    std::size_t offset = 0;
    while (offset < contents.size()) {
        const std::size_t count = std::min(
            contents.size() - offset,
            static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()));
        output.write(contents.data() + offset, static_cast<std::streamsize>(count));
        if (!output) {
            throw std::runtime_error("Failed to write output file: " + output_name);
        }
        offset += count;
    }

    output.flush();
    if (!output) {
        throw std::runtime_error("Failed to write output file: " + output_name);
    }
}

void Archivator::user_compress(const std::string& output_name)
{
    compression->compress();
    save(output_name);
}

void Archivator::user_decompress(
    const std::string& input_name,
    const std::string& output_name)
{
    std::ifstream input(input_name, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open input file: " + input_name);
    }

    std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (input.bad()) {
        throw std::runtime_error("Failed to read input file: " + input_name);
    }

    compression->load(contents);
    compression->decompress();
    save(output_name);
}
