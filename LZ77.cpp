#include "LZ77.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::array<unsigned char, 4> signature{{'L', '7', '7', 1}};
constexpr std::size_t header_size = 12;
constexpr std::size_t window_size = 32768;
constexpr std::size_t minimum_match = 3;
constexpr std::size_t maximum_match = 258;
constexpr std::size_t maximum_chain_checks = 64;
constexpr std::size_t hash_size = 1 << 16;
constexpr std::size_t no_position = std::numeric_limits<std::size_t>::max();

std::size_t hash_at(const std::string& input, std::size_t position)
{
    const auto first = static_cast<unsigned char>(input[position]);
    const auto second = static_cast<unsigned char>(input[position + 1]);
    const auto third = static_cast<unsigned char>(input[position + 2]);
    return ((static_cast<std::size_t>(first) * 251u + second) * 251u + third) & (hash_size - 1);
}

void append_u16(std::string& output, std::size_t value)
{
    output.push_back(static_cast<char>(value));
    output.push_back(static_cast<char>(value >> 8));
}

void append_u64(std::string& output, std::uint64_t value)
{
    for (unsigned int shift = 0; shift < 64; shift += 8) {
        output.push_back(static_cast<char>(value >> shift));
    }
}

std::uint64_t read_u64(const std::string& input, std::size_t position)
{
    std::uint64_t value = 0;
    for (unsigned int index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(
            static_cast<unsigned char>(input[position + index])) << (index * 8);
    }
    return value;
}

} // namespace

void LZ77::compress()
{
    result.assign(signature.begin(), signature.end());
    append_u64(result, static_cast<std::uint64_t>(data.size()));

    std::vector<std::size_t> head(hash_size, no_position);
    std::vector<std::size_t> previous(data.size(), no_position);
    std::size_t position = 0;

    while (position < data.size()) {
        std::size_t best_length = 0;
        std::size_t best_distance = 0;

        if (data.size() - position >= minimum_match) {
            std::size_t candidate = head[hash_at(data, position)];
            std::size_t checks = 0;
            const std::size_t limit = std::min(data.size() - position, maximum_match);

            while (candidate != no_position && checks < maximum_chain_checks) {
                const std::size_t distance = position - candidate;
                if (distance > window_size) {
                    break;
                }
                ++checks;

                if (data[candidate] == data[position]
                    && data[candidate + 1] == data[position + 1]
                    && data[candidate + 2] == data[position + 2]) {
                    std::size_t length = minimum_match;
                    while (length < limit && data[candidate + length] == data[position + length]) {
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
            result.push_back(1);
            append_u16(result, best_distance);
            append_u16(result, best_length);
        } else {
            result.push_back(0);
            result.push_back(data[position]);
        }

        const std::size_t next_position = position + consumed;
        for (std::size_t index = position; index < next_position; ++index) {
            if (data.size() - index < minimum_match) {
                break;
            }
            const std::size_t hash = hash_at(data, index);
            previous[index] = head[hash];
            head[hash] = index;
        }
        position = next_position;
    }
}

void LZ77::decompress()
{
    if (data.size() < header_size
        || !std::equal(signature.begin(), signature.end(), data.begin(),
            [](unsigned char expected, char actual) {
                return expected == static_cast<unsigned char>(actual);
            })) {
        throw std::invalid_argument("Invalid or incomplete LZ77 header");
    }

    const std::uint64_t expected_size = read_u64(data, signature.size());
    if (expected_size > std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("LZ77 output is too large for this platform");
    }
    const std::size_t output_size = static_cast<std::size_t>(expected_size);
    result.clear();
    result.reserve(output_size);

    std::size_t position = header_size;
    while (position < data.size()) {
        const auto token = static_cast<unsigned char>(data[position++]);
        if (token == 0) {
            if (position == data.size() || result.size() == output_size) {
                throw std::invalid_argument("Invalid LZ77 literal");
            }
            result.push_back(data[position++]);
            continue;
        }

        if (token != 1 || data.size() - position < 4) {
            throw std::invalid_argument("Invalid or incomplete LZ77 match");
        }
        const std::size_t distance = static_cast<unsigned char>(data[position])
            | (static_cast<std::size_t>(static_cast<unsigned char>(data[position + 1])) << 8);
        const std::size_t length = static_cast<unsigned char>(data[position + 2])
            | (static_cast<std::size_t>(static_cast<unsigned char>(data[position + 3])) << 8);
        position += 4;

        if (distance == 0 || distance > window_size || distance > result.size()
            || length < minimum_match || length > maximum_match
            || length > output_size - result.size()) {
            throw std::invalid_argument("Invalid LZ77 match distance or length");
        }
        for (std::size_t index = 0; index < length; ++index) {
            result.push_back(result[result.size() - distance]);
        }
    }

    if (result.size() != output_size) {
        throw std::invalid_argument("LZ77 output does not match its declared size");
    }
}
