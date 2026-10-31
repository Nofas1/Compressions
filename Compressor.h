#pragma once

#include <string>

class Compressor
{
protected:
    std::string data;
    std::string result;

public:
    virtual ~Compressor() = default;

    virtual void compress() = 0;
    virtual void decompress() = 0;

    virtual void load(const std::string& buffer)
    {
        data = buffer;
        result.clear();
    }

    const std::string& get_result() const noexcept
    {
        return result;
    }
};
