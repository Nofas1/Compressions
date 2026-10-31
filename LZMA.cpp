#include "LZMA.h"


void LZMA::deltaEncoder()
{
    delta.push_back(data[0]);
    for(int i = 1; i < data.size(); ++i) {
        char current_differ = data[i] - data[i - 1];
        delta.push_back(current_differ);
    }
}

LZMA::LZMA() {}

void LZMA::compress()
{
    deltaEncoder();
    int l_buffer = preCheck().first, r_buffer = preCheck().second;
    afterSolve(l_buffer, r_buffer);
    
}

my_types::pii LZMA::preCheck()
{
    int l_buffer = 0, r_buffer = 0;
    int size = std::min(my_types::size_of_window, static_cast<int>(delta.size()));
    int pos = 0;

    while (pos < size) {
        int offset = 0;
        int length = 1;
        shiftBuffer(l_buffer, r_buffer, length);
        pos += length;
        used.push_back({offset, length, delta[pos]});
    }
    return {l_buffer, r_buffer};
}

void LZMA::afterSolve(int l_buffer, int r_buffer)
{
    int pos = used.size() - 1;
    while (pos < delta.size()) {
        my_types::pii curr = findMatching(pos);
        int offset = curr.first;
        int length = curr.second;
        shiftBuffer(l_buffer, r_buffer, length + 1);
        pos += length;
        used.push_back({offset, length, delta[pos]});
    }
}

// void LZMA::preCheck()
// {
//     int l_buffer = 0, r_buffer = 0;
//     int size = std::min(my_types::size_of_window, static_cast<int>(delta.size()));
//     int pos = 0;

//     while (pos < delta.size()) {
//         my_types::pii curr = findMatching(pos);
//         int offset = curr.first;
//         int length = curr.second;
//         shiftBuffer(l_buffer, r_buffer, length + 1);
//         pos += length;
//         used.push_back({offset, length, delta[pos]});
//     }
// }

my_types::pii LZMA::findMatching(int pos)
{
    if (pos >= delta.size()) {
        return {0, 0};
    }
    int bestMatchLength = 0;
    int bestMatchOffset = 0;

    for (int i = std::max(0, pos - my_types::BUFFER_SIZE); i < pos; ++i) {
        int matchLength = 0;

        while (matchLength + pos < delta.size() and delta[i + matchLength] == delta[pos + matchLength]) {
            ++matchLength;
            if (matchLength == my_types::BUFFER_SIZE) {
                break;
            }
        }
        if (bestMatchLength < matchLength) {
            bestMatchLength = matchLength;
            bestMatchOffset = pos - i;
        }
    }
    return {bestMatchOffset, bestMatchLength};
}

void LZMA::shiftBuffer(int l, int r, int x)
{
    if (r + x <= delta.size()) {
        if (r - l < my_types::BUFFER_SIZE) {
            r += x;
        } else {
            l += x;
            r += x;
        }
    }
}

void LZMA::decompress()
{
    std::string new_data = "";
    std::string after_delta = "";
    for (Node& node : used) {
        if (node.length) {
            int first = node.length - node.offset;
            for (int i = 0; i <= node.length - 1; ++i) {
                after_delta += data[first + i];
            }
        }
        after_delta += node.next;
    }

    new_data += after_delta[0];
    for (int i = 1; i < after_delta.size(); ++i) {
        char prev = after_delta[0] + static_cast<int>(after_delta[i]);
        new_data += prev;
    }
    
    // return new_data;
}



