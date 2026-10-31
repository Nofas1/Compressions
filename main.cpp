#include "Archivator.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

void print_usage(std::ostream& output)
{
    output << "Usage:\n"
           << "  archivator compress <deflate|lz77> <input-file> <output-file>\n"
           << "  archivator decompress <deflate|lz77> <input-file> <output-file>\n";
}

my_types::ChooseYourCompression parse_algorithm(const std::string& value)
{
    if (value == "deflate" || value == "Deflate" || value == "3") {
        return my_types::Deflate;
    }
    if (value == "lz77" || value == "LZ77" || value == "1") {
        return my_types::LZ77;
    }
    if (value == "lzma" || value == "LZMA" || value == "2") {
        throw std::invalid_argument("LZMA is not implemented; choose Deflate or LZ77.");
    }
    throw std::invalid_argument("Unknown algorithm: " + value);
}

std::string read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Cannot open input file: " + path);
    }

    std::string contents{
        std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>()};
    if (input.bad()) {
        throw std::runtime_error("Failed to read input file: " + path);
    }
    return contents;
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 5) {
        print_usage(std::cerr);
        return 2;
    }

    const std::string operation = argv[1];
    if (operation != "compress" && operation != "decompress") {
        std::cerr << "Unknown operation: " << operation << '\n';
        print_usage(std::cerr);
        return 2;
    }

    try {
        Archivator archivator(parse_algorithm(argv[2]));
        if (operation == "compress") {
            const std::string input = read_file(argv[3]);
            archivator.send_buffer(input);
            archivator.user_compress(argv[4]);
        } else {
            archivator.user_decompress(argv[3], argv[4]);
        }

        std::cout << (operation == "compress" ? "Compression" : "Decompression")
                  << " completed: " << argv[4] << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
