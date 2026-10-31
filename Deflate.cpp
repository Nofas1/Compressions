#include "Deflate.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint32_t adler_modulus = 65521;
constexpr std::size_t window_size = 32768;
constexpr std::size_t minimum_match = 3;
constexpr std::size_t maximum_match = 258;
constexpr std::size_t maximum_chain_checks = 128;
constexpr std::size_t hash_size = 1 << 16;
constexpr std::size_t no_position = std::numeric_limits<std::size_t>::max();

constexpr std::array<unsigned short, 29> length_base{{
    3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
}};

constexpr std::array<unsigned char, 29> length_extra{{
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
}};

constexpr std::array<unsigned short, 30> distance_base{{
    1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
    6145, 8193, 12289, 16385, 24577
}};

constexpr std::array<unsigned char, 30> distance_extra{{
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
}};

constexpr std::array<unsigned char, 19> code_length_order{{
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
}};

unsigned int reverse_bits(unsigned int value, unsigned int count)
{
    unsigned int reversed = 0;
    for (unsigned int bit = 0; bit < count; ++bit) {
        reversed = (reversed << 1) | (value & 1u);
        value >>= 1;
    }
    return reversed;
}

class BitWriter {
public:
    void write_bits(unsigned int value, unsigned int count)
    {
        for (unsigned int bit = 0; bit < count; ++bit) {
            current_ |= static_cast<unsigned char>(((value >> bit) & 1u) << bit_count_);
            if (++bit_count_ == 8) {
                bytes_.push_back(static_cast<char>(current_));
                current_ = 0;
                bit_count_ = 0;
            }
        }
    }

    void write_huffman(unsigned int code, unsigned int length)
    {
        write_bits(reverse_bits(code, length), length);
    }

    void align_to_byte()
    {
        if (bit_count_ != 0) {
            bytes_.push_back(static_cast<char>(current_));
            current_ = 0;
            bit_count_ = 0;
        }
    }

    const std::string& bytes() const noexcept
    {
        return bytes_;
    }

private:
    std::string bytes_;
    unsigned char current_ = 0;
    unsigned int bit_count_ = 0;
};

class BitReader {
public:
    BitReader(const std::string& bytes, std::size_t first_byte, std::size_t last_byte)
        : bytes_(bytes), bit_position_(first_byte * 8), bit_limit_(last_byte * 8)
    {
    }

    unsigned int read_bits(unsigned int count)
    {
        if (count > 24 || count > bit_limit_ - bit_position_) {
            throw std::invalid_argument("Incomplete DEFLATE bitstream");
        }

        unsigned int value = 0;
        for (unsigned int bit = 0; bit < count; ++bit) {
            const auto byte = static_cast<unsigned char>(bytes_[bit_position_ / 8]);
            value |= static_cast<unsigned int>((byte >> (bit_position_ % 8)) & 1u) << bit;
            ++bit_position_;
        }
        return value;
    }

    void align_to_byte()
    {
        const std::size_t padding = (8 - bit_position_ % 8) % 8;
        if (padding > bit_limit_ - bit_position_) {
            throw std::invalid_argument("Incomplete DEFLATE byte alignment");
        }
        bit_position_ += padding;
    }

    std::size_t byte_position() const noexcept
    {
        return bit_position_ / 8;
    }

private:
    const std::string& bytes_;
    std::size_t bit_position_;
    std::size_t bit_limit_;
};

class HuffmanTree {
public:
    explicit HuffmanTree(const std::vector<unsigned char>& lengths, unsigned int maximum_bits)
        : counts_(maximum_bits + 1, 0),
          first_code_(maximum_bits + 1, 0),
          first_symbol_(maximum_bits + 1, 0)
    {
        for (unsigned char length : lengths) {
            if (length > maximum_bits) {
                throw std::invalid_argument("Invalid Huffman code length");
            }
            if (length != 0) {
                ++counts_[length];
            }
        }

        int remaining_codes = 1;
        for (unsigned int length = 1; length <= maximum_bits; ++length) {
            remaining_codes = (remaining_codes << 1) - counts_[length];
            if (remaining_codes < 0) {
                throw std::invalid_argument("Oversubscribed Huffman tree");
            }
        }

        unsigned int next_code = 0;
        unsigned int symbol_offset = 0;
        for (unsigned int length = 1; length <= maximum_bits; ++length) {
            next_code = (next_code + counts_[length - 1]) << 1;
            first_code_[length] = next_code;
            first_symbol_[length] = symbol_offset;
            symbol_offset += counts_[length];
        }

        symbols_.reserve(symbol_offset);
        for (unsigned int length = 1; length <= maximum_bits; ++length) {
            for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
                if (lengths[symbol] == length) {
                    symbols_.push_back(static_cast<unsigned short>(symbol));
                }
            }
        }
        maximum_bits_ = maximum_bits;
    }

    unsigned int decode(BitReader& reader) const
    {
        unsigned int code = 0;
        for (unsigned int length = 1; length <= maximum_bits_; ++length) {
            code = (code << 1) | reader.read_bits(1);
            const unsigned int first = first_code_[length];
            if (code >= first && code - first < counts_[length]) {
                return symbols_[first_symbol_[length] + code - first];
            }
        }
        throw std::invalid_argument("Invalid Huffman code");
    }

private:
    std::vector<unsigned int> counts_;
    std::vector<unsigned int> first_code_;
    std::vector<unsigned int> first_symbol_;
    std::vector<unsigned short> symbols_;
    unsigned int maximum_bits_ = 0;
};

std::vector<unsigned char> fixed_literal_lengths()
{
    std::vector<unsigned char> lengths(288);
    for (unsigned int symbol = 0; symbol <= 143; ++symbol) {
        lengths[symbol] = 8;
    }
    for (unsigned int symbol = 144; symbol <= 255; ++symbol) {
        lengths[symbol] = 9;
    }
    for (unsigned int symbol = 256; symbol <= 279; ++symbol) {
        lengths[symbol] = 7;
    }
    for (unsigned int symbol = 280; symbol <= 287; ++symbol) {
        lengths[symbol] = 8;
    }
    return lengths;
}

unsigned int fixed_code(unsigned int symbol, unsigned int& length)
{
    if (symbol <= 143) {
        length = 8;
        return 0x30u + symbol;
    }
    if (symbol <= 255) {
        length = 9;
        return 0x190u + symbol - 144;
    }
    if (symbol <= 279) {
        length = 7;
        return symbol - 256;
    }
    length = 8;
    return 0xc0u + symbol - 280;
}

std::size_t hash_at(const std::string& input, std::size_t position)
{
    const auto first = static_cast<unsigned char>(input[position]);
    const auto second = static_cast<unsigned char>(input[position + 1]);
    const auto third = static_cast<unsigned char>(input[position + 2]);
    return ((static_cast<std::size_t>(first) * 251u + second) * 251u + third) & (hash_size - 1);
}

void add_lz77_match(BitWriter& writer, std::size_t length, std::size_t distance)
{
    std::size_t length_code = 0;
    while (length_code + 1 < length_base.size()
        && length >= length_base[length_code + 1]) {
        ++length_code;
    }

    unsigned int code_length = 0;
    const unsigned int code = fixed_code(
        static_cast<unsigned int>(257 + length_code), code_length);
    writer.write_huffman(code, code_length);
    const unsigned int length_bits = length_extra[length_code];
    if (length_bits != 0) {
        writer.write_bits(static_cast<unsigned int>(length - length_base[length_code]), length_bits);
    }

    std::size_t distance_code = 0;
    while (distance_code + 1 < distance_base.size()
        && distance >= distance_base[distance_code + 1]) {
        ++distance_code;
    }

    writer.write_huffman(static_cast<unsigned int>(distance_code), 5);
    const unsigned int distance_bits = distance_extra[distance_code];
    if (distance_bits != 0) {
        writer.write_bits(
            static_cast<unsigned int>(distance - distance_base[distance_code]),
            distance_bits);
    }
}

std::string encode_fixed_block(const std::string& input)
{
    BitWriter writer;
    writer.write_bits(1, 1);
    writer.write_bits(1, 2);

    std::vector<std::size_t> head(hash_size, no_position);
    std::vector<std::size_t> previous(input.size(), no_position);

    std::size_t position = 0;
    while (position < input.size()) {
        std::size_t best_length = 0;
        std::size_t best_distance = 0;
        const std::size_t remaining = input.size() - position;

        if (remaining >= minimum_match) {
            std::size_t candidate = head[hash_at(input, position)];
            std::size_t checks = 0;
            const std::size_t limit = std::min(remaining, maximum_match);

            while (candidate != no_position && checks < maximum_chain_checks) {
                const std::size_t distance = position - candidate;
                if (distance > window_size) {
                    break;
                }
                ++checks;

                if (input[candidate] == input[position]
                    && input[candidate + 1] == input[position + 1]
                    && input[candidate + 2] == input[position + 2]) {
                    std::size_t length = minimum_match;
                    while (length < limit && input[candidate + length] == input[position + length]) {
                        ++length;
                    }
                    if (length > best_length) {
                        best_length = length;
                        best_distance = distance;
                        if (length == limit) {
                            break;
                        }
                    }
                }

                candidate = previous[candidate];
            }
        }

        const std::size_t consumed = best_length >= minimum_match ? best_length : 1;
        if (best_length >= minimum_match) {
            add_lz77_match(writer, best_length, best_distance);
        } else {
            unsigned int code_length = 0;
            const unsigned int literal = static_cast<unsigned char>(input[position]);
            const unsigned int code = fixed_code(literal, code_length);
            writer.write_huffman(code, code_length);
        }

        const std::size_t next_position = position + consumed;
        for (std::size_t index = position; index < next_position; ++index) {
            if (input.size() - index < minimum_match) {
                break;
            }
            const std::size_t hash = hash_at(input, index);
            previous[index] = head[hash];
            head[hash] = index;
        }
        position = next_position;
    }

    unsigned int end_code_length = 0;
    const unsigned int end_code = fixed_code(256, end_code_length);
    writer.write_huffman(end_code, end_code_length);
    writer.align_to_byte();
    return writer.bytes();
}

std::string encode_stored_blocks(const std::string& input)
{
    BitWriter writer;
    std::size_t position = 0;

    do {
        const std::size_t length = std::min<std::size_t>(input.size() - position, 65535);
        const bool final_block = position + length == input.size();
        writer.write_bits(final_block ? 1u : 0u, 1);
        writer.write_bits(0, 2);
        writer.align_to_byte();
        writer.write_bits(static_cast<unsigned int>(length), 16);
        writer.write_bits(static_cast<unsigned int>(length ^ 0xffffu), 16);
        for (std::size_t index = 0; index < length; ++index) {
            writer.write_bits(static_cast<unsigned char>(input[position + index]), 8);
        }
        position += length;
    } while (position < input.size());

    writer.align_to_byte();
    return writer.bytes();
}

std::uint32_t adler32(const std::string& input)
{
    std::uint32_t first = 1;
    std::uint32_t second = 0;
    for (unsigned char byte : input) {
        first = (first + byte) % adler_modulus;
        second = (second + first) % adler_modulus;
    }
    return (second << 16) | first;
}

void append_uint32_be(std::string& output, std::uint32_t value)
{
    output.push_back(static_cast<char>(value >> 24));
    output.push_back(static_cast<char>(value >> 16));
    output.push_back(static_cast<char>(value >> 8));
    output.push_back(static_cast<char>(value));
}

std::uint32_t read_uint32_be(const std::string& input, std::size_t position)
{
    return (static_cast<std::uint32_t>(static_cast<unsigned char>(input[position])) << 24)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(input[position + 1])) << 16)
        | (static_cast<std::uint32_t>(static_cast<unsigned char>(input[position + 2])) << 8)
        | static_cast<std::uint32_t>(static_cast<unsigned char>(input[position + 3]));
}

unsigned int decode_length(unsigned int symbol, BitReader& reader)
{
    if (symbol < 257 || symbol > 285) {
        throw std::invalid_argument("Reserved DEFLATE length code");
    }
    const std::size_t index = symbol - 257;
    return length_base[index] + reader.read_bits(length_extra[index]);
}

unsigned int decode_distance(BitReader& reader, const HuffmanTree& tree)
{
    const unsigned int symbol = tree.decode(reader);
    if (symbol >= distance_base.size()) {
        throw std::invalid_argument("Reserved DEFLATE distance code");
    }
    return distance_base[symbol] + reader.read_bits(distance_extra[symbol]);
}

void decode_compressed_block(
    BitReader& reader,
    const HuffmanTree& literal_tree,
    const HuffmanTree& distance_tree,
    std::string& output)
{
    while (true) {
        const unsigned int symbol = literal_tree.decode(reader);
        if (symbol < 256) {
            output.push_back(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256) {
            return;
        }

        const unsigned int length = decode_length(symbol, reader);
        const unsigned int distance = decode_distance(reader, distance_tree);
        if (distance == 0 || distance > output.size()) {
            throw std::invalid_argument("DEFLATE match refers beyond the output window");
        }
        for (unsigned int index = 0; index < length; ++index) {
            output.push_back(output[output.size() - distance]);
        }
    }
}

void decode_dynamic_block(BitReader& reader, std::string& output)
{
    const unsigned int literal_count = reader.read_bits(5) + 257;
    const unsigned int distance_count = reader.read_bits(5) + 1;
    const unsigned int code_length_count = reader.read_bits(4) + 4;
    if (literal_count > 286 || distance_count > 32) {
        throw std::invalid_argument("Invalid DEFLATE dynamic Huffman header");
    }

    std::vector<unsigned char> code_length_lengths(19, 0);
    for (unsigned int index = 0; index < code_length_count; ++index) {
        code_length_lengths[code_length_order[index]] = static_cast<unsigned char>(reader.read_bits(3));
    }
    const HuffmanTree code_length_tree(code_length_lengths, 7);

    std::vector<unsigned char> combined_lengths;
    combined_lengths.reserve(literal_count + distance_count);
    while (combined_lengths.size() < literal_count + distance_count) {
        const unsigned int symbol = code_length_tree.decode(reader);
        if (symbol <= 15) {
            combined_lengths.push_back(static_cast<unsigned char>(symbol));
            continue;
        }

        unsigned int repeat_count = 0;
        unsigned char repeated_value = 0;
        if (symbol == 16) {
            if (combined_lengths.empty()) {
                throw std::invalid_argument("Invalid repeat in DEFLATE code lengths");
            }
            repeat_count = reader.read_bits(2) + 3;
            repeated_value = combined_lengths.back();
        } else if (symbol == 17) {
            repeat_count = reader.read_bits(3) + 3;
        } else if (symbol == 18) {
            repeat_count = reader.read_bits(7) + 11;
        } else {
            throw std::invalid_argument("Invalid DEFLATE code-length symbol");
        }

        if (repeat_count > literal_count + distance_count - combined_lengths.size()) {
            throw std::invalid_argument("DEFLATE code-length repeat exceeds its table");
        }
        combined_lengths.insert(combined_lengths.end(), repeat_count, repeated_value);
    }

    std::vector<unsigned char> literal_lengths(
        combined_lengths.begin(), combined_lengths.begin() + literal_count);
    if (literal_lengths[256] == 0) {
        throw std::invalid_argument("DEFLATE literal tree has no end-of-block code");
    }
    std::vector<unsigned char> distance_lengths(
        combined_lengths.begin() + literal_count, combined_lengths.end());

    const HuffmanTree literal_tree(literal_lengths, 15);
    const HuffmanTree distance_tree(distance_lengths, 15);
    decode_compressed_block(reader, literal_tree, distance_tree, output);
}

std::string decode_zlib_stream(const std::string& input)
{
    if (input.size() < 6) {
        throw std::invalid_argument("Incomplete zlib-wrapped DEFLATE stream");
    }

    const auto cmf = static_cast<unsigned char>(input[0]);
    const auto flags = static_cast<unsigned char>(input[1]);
    if ((cmf & 0x0f) != 8 || (cmf >> 4) > 7
        || ((static_cast<unsigned int>(cmf) << 8) | flags) % 31 != 0
        || (flags & 0x20) != 0) {
        throw std::invalid_argument("Invalid or unsupported zlib header");
    }

    const std::size_t payload_end = input.size() - 4;
    BitReader reader(input, 2, payload_end);
    std::string output;
    bool final_block = false;

    do {
        final_block = reader.read_bits(1) != 0;
        const unsigned int block_type = reader.read_bits(2);
        if (block_type == 0) {
            reader.align_to_byte();
            const unsigned int length = reader.read_bits(16);
            const unsigned int inverse_length = reader.read_bits(16);
            if ((length ^ inverse_length) != 0xffffu) {
                throw std::invalid_argument("Invalid DEFLATE stored-block length");
            }
            for (unsigned int index = 0; index < length; ++index) {
                output.push_back(static_cast<char>(reader.read_bits(8)));
            }
        } else if (block_type == 1) {
            const HuffmanTree literal_tree(fixed_literal_lengths(), 15);
            const HuffmanTree distance_tree(std::vector<unsigned char>(32, 5), 15);
            decode_compressed_block(reader, literal_tree, distance_tree, output);
        } else if (block_type == 2) {
            decode_dynamic_block(reader, output);
        } else {
            throw std::invalid_argument("Reserved DEFLATE block type");
        }
    } while (!final_block);

    reader.align_to_byte();
    if (reader.byte_position() != payload_end) {
        throw std::invalid_argument("Trailing data in DEFLATE stream");
    }
    if (read_uint32_be(input, payload_end) != adler32(output)) {
        throw std::invalid_argument("DEFLATE checksum mismatch");
    }
    return output;
}

} // namespace

void Deflate::compress()
{
    const std::string fixed = encode_fixed_block(data);
    const std::string stored = encode_stored_blocks(data);
    const std::string& payload = fixed.size() <= stored.size() ? fixed : stored;

    result.clear();
    result.reserve(payload.size() + 6);
    result.push_back(static_cast<char>(0x78));
    result.push_back(static_cast<char>(0x01));
    result.append(payload);
    append_uint32_be(result, adler32(data));
}

void Deflate::decompress()
{
    result = decode_zlib_stream(data);
}
