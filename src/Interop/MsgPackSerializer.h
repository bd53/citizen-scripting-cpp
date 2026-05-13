#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "../Json.h"

namespace fx::msgpack
{

namespace detail
{

inline void writeU8 (std::vector<uint8_t>& b, uint8_t v)  { b.push_back(v); }

inline void writeU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}

inline void writeU32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(static_cast<uint8_t>(v >> 24));
    b.push_back(static_cast<uint8_t>(v >> 16));
    b.push_back(static_cast<uint8_t>(v >> 8));
    b.push_back(static_cast<uint8_t>(v));
}

inline void writeU64(std::vector<uint8_t>& b, uint64_t v)
{
    writeU32(b, static_cast<uint32_t>(v >> 32));
    writeU32(b, static_cast<uint32_t>(v));
}

inline void writeStr(std::vector<uint8_t>& b, const std::string& s)
{
    size_t n = s.size();
    if (n <= 31)
    {
        b.push_back(static_cast<uint8_t>(0xA0 | n));
    }
    else if (n <= 255)
    {
        b.push_back(0xD9);
        b.push_back(static_cast<uint8_t>(n));
    }
    else if (n <= 65535)
    {
        b.push_back(0xDA);
        writeU16(b, static_cast<uint16_t>(n));
    }
    else
    {
        b.push_back(0xDB);
        writeU32(b, static_cast<uint32_t>(n));
    }
    b.insert(b.end(), s.begin(), s.end());
}

inline void writeValue(std::vector<uint8_t>& b, const json::Value& v)
{
    switch (v.kind)
    {
    case json::Value::Kind::Null:
        b.push_back(0xC0);
        break;

    case json::Value::Kind::Bool:
        b.push_back(v.scalar == "true" ? 0xC3 : 0xC2);
        break;

    case json::Value::Kind::Number: {
        char* end = nullptr;
        long long ival = std::strtoll(v.scalar.c_str(), &end, 10);
        if (end && *end == '\0')
        {
            if (ival >= 0 && ival <= 127)
            {
                b.push_back(static_cast<uint8_t>(ival));
            }
            else if (ival < 0 && ival >= -32)
            {
                b.push_back(static_cast<uint8_t>(ival));
            }
            else if (ival >= -128 && ival <= 127)
            {
                b.push_back(0xD0); b.push_back(static_cast<uint8_t>(ival));
            }
            else if (ival >= -32768 && ival <= 32767)
            {
                b.push_back(0xD1); writeU16(b, static_cast<uint16_t>(static_cast<int16_t>(ival)));
            }
            else if (ival >= -2147483648LL && ival <= 2147483647LL)
            {
                b.push_back(0xD2); writeU32(b, static_cast<uint32_t>(static_cast<int32_t>(ival)));
            }
            else
            {
                b.push_back(0xD3); writeU64(b, static_cast<uint64_t>(ival));
            }
        }
        else
        {
            double d = std::strtod(v.scalar.c_str(), nullptr);
            uint64_t bits; memcpy(&bits, &d, 8);
            b.push_back(0xCB);
            b.push_back(static_cast<uint8_t>(bits >> 56));
            b.push_back(static_cast<uint8_t>(bits >> 48));
            b.push_back(static_cast<uint8_t>(bits >> 40));
            b.push_back(static_cast<uint8_t>(bits >> 32));
            b.push_back(static_cast<uint8_t>(bits >> 24));
            b.push_back(static_cast<uint8_t>(bits >> 16));
            b.push_back(static_cast<uint8_t>(bits >> 8));
            b.push_back(static_cast<uint8_t>(bits));
        }
        break;
    }

    case json::Value::Kind::String:
        writeStr(b, v.scalar);
        break;

    case json::Value::Kind::Array: {
        size_t n = v.children.size();
        if (n <= 15)
            b.push_back(static_cast<uint8_t>(0x90 | n));
        else if (n <= 65535)
        {
            b.push_back(0xDC);
            writeU16(b, static_cast<uint16_t>(n));
        }
        else
        {
            b.push_back(0xDD);
            writeU32(b, static_cast<uint32_t>(n));
        }
        for (auto& child : v.children)
            writeValue(b, child);
        break;
    }

    case json::Value::Kind::Object: {
        size_t n = v.fields.size();
        if (n <= 15)
            b.push_back(static_cast<uint8_t>(0x80 | n));
        else if (n <= 65535)
        {
            b.push_back(0xDE);
            writeU16(b, static_cast<uint16_t>(n));
        }
        else
        {
            b.push_back(0xDF);
            writeU32(b, static_cast<uint32_t>(n));
        }
        for (auto& [key, val] : v.fields)
        {
            writeStr(b, key);
            writeValue(b, val);
        }
        break;
    }

    case json::Value::Kind::FuncRef: {
        size_t n = v.scalar.size();
        if (n <= 255)
        {
            b.push_back(0xC7);
            b.push_back(static_cast<uint8_t>(n));
        }
        else
        {
            b.push_back(0xC8);
            writeU16(b, static_cast<uint16_t>(n));
        }
        b.push_back(10); // function reference ext type
        b.insert(b.end(), v.scalar.begin(), v.scalar.end());
        break;
    }

    default:
        b.push_back(0xC0);
        break;
    }
}

}

inline std::vector<uint8_t> encode(const json::Value& v)
{
    std::vector<uint8_t> buf;
    detail::writeValue(buf, v);
    return buf;
}

}
