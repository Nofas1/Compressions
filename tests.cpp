#include "Deflate.h"
#include "LZ77.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string round_trip(Compressor& encoder, Compressor& decoder, const std::string& input)
{
    encoder.load(input);
    encoder.compress();
    const std::string compressed = encoder.get_result();
    decoder.load(compressed);
    decoder.decompress();
    return decoder.get_result();
}

void expect_invalid_deflate(const std::string& input)
{
    Deflate decoder;
    decoder.load(input);
    try {
        decoder.decompress();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("Malformed DEFLATE data was accepted");
}

std::string bytes_from_hex(const std::string& hex)
{
    require(hex.size() % 2 == 0, "Invalid hexadecimal DEFLATE test fixture");
    std::string bytes;
    bytes.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
        const unsigned int value = static_cast<unsigned int>(
            std::stoul(hex.substr(index, 2), nullptr, 16));
        bytes.push_back(static_cast<char>(value));
    }
    return bytes;
}

void test_deflate_round_trips()
{
    Deflate encoder;
    Deflate decoder;
    const std::vector<std::string> samples{
        "",
        std::string("\0", 1),
        std::string("one\0two\0three", 13),
        std::string(200000, 'A'),
        "abracadabra abracadabra abracadabra"
    };

    for (std::size_t index = 0; index < samples.size(); ++index) {
        require(round_trip(encoder, decoder, samples[index]) == samples[index],
            "DEFLATE round trip failed for sample " + std::to_string(index));
    }

    std::string all_bytes;
    for (unsigned int value = 0; value < 256; ++value) {
        all_bytes.push_back(static_cast<char>(value));
    }
    require(round_trip(encoder, decoder, all_bytes) == all_bytes, "Binary DEFLATE round trip failed");

    const std::string repetitive(200000, 'A');
    encoder.load(repetitive);
    encoder.compress();
    require(encoder.get_result().size() < repetitive.size(), "DEFLATE did not compress repetitive input");
}

void test_dynamic_huffman_block()
{
    const std::string compressed = bytes_from_hex(
        "789c1d8ead2e056000868bc4cc1447228b2ae9797f149140926426d8045538db"
        "99098a4d9165a2cd347720d9dc812be03bedfddbb3d7093b44c52b3ead411fa"
        "5fb266d5a7de732aadb6ef37a150e2c2dad3b58f7b384a4b59eaa997373e687"
        "3827c458effcc3b286cf173b27e44571adc1c59f8f3d340b9d1ed370d4a0df"
        "8b5cbf4d5c23d785b1dc184d19d1dc3b5a6577f4e50ea0d216b8b1c29e8260"
        "7cea10066f56d618dc8eb7c27af617cbf887fe01352e4b5e");

    std::string expected(256, '\0');
    std::uint32_t value = 0x12345678;
    for (char& byte : expected) {
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        byte = static_cast<char>(
            value % 10 < 8 ? 65 + ((value >> 8) % 5) : ((value >> 16) & 0xff));
    }

    Deflate decoder;
    decoder.load(compressed);
    decoder.decompress();
    require(decoder.get_result() == expected, "Dynamic Huffman DEFLATE fixture did not decode");
}

void test_deflate_rejects_bad_input()
{
    expect_invalid_deflate("");
    expect_invalid_deflate(std::string("\x78", 1));

    Deflate encoder;
    encoder.load("checksum");
    encoder.compress();
    std::string bad_checksum = encoder.get_result();
    bad_checksum.back() ^= 1;
    expect_invalid_deflate(bad_checksum);

    std::string truncated = encoder.get_result();
    truncated.pop_back();
    expect_invalid_deflate(truncated);
}

void test_lz77_round_trips()
{
    LZ77 encoder;
    LZ77 decoder;
    const std::vector<std::string> samples{
        "",
        std::string("a\0b\0c", 5),
        std::string(100000, 'x'),
        "abcabcabcabc abcabcabcabc"
    };

    for (const std::string& input : samples) {
        require(round_trip(encoder, decoder, input) == input, "LZ77 round trip failed");
    }
}

} // namespace

int main()
{
    try {
        test_deflate_round_trips();
        test_dynamic_huffman_block();
        test_deflate_rejects_bad_input();
        test_lz77_round_trips();
    } catch (const std::exception& error) {
        std::cerr << "Test failed: " << error.what() << '\n';
        return 1;
    }

    std::cout << "All compression tests passed.\n";
    return 0;
}
