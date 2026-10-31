#pragma once

#include "My_types.h"
#include "Compressor.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <map>


class LZMA : public Compressor
{
    struct Node {
        int offset;
        int length;
        char next;
    };

    std::vector<Node> used;
    std::vector<char> delta;

public:

    // reformating data into vector containing differece with the first char, while first element remains intact
    void deltaEncoder();

    LZMA();

    void compress();

    my_types::pii preCheck();

    void afterSolve(int, int);

    my_types::pii findMatching(int);

    void shiftBuffer(int, int, int);

    void decompress();
};
