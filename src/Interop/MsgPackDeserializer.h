#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <stdexcept>

#include "../Json.h"

namespace fx::msgpack
{

namespace detail
{

struct Reader
{
    const uint8_t* p;
    const uint8_t* end;

    uint8_t u8()
    {
        if (p >= end) throw std::runtime_error("msgpack: truncated");
        return *p++;
    }

    uint16_t u16()
    {
        uint16_t v;
        if (p + 2 > end) throw std::runtime_error("msgpack: truncated");
        memcpy(&v, p, 2); p += 2;
        return static_cast<uint16_t>((v >> 8) | (v << 8));
    }

    uint32_t u32()
    {
        uint32_t v;
        if (p + 4 > end) throw std::runtime_error("msgpack: truncated");
        memcpy(&v, p, 4); p += 4;
        v = ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000u);
        return v;
    }

    uint64_t u64()
    {
        uint32_t hi = u32(), lo = u32();
        return (static_cast<uint64_t>(hi) << 32) | lo;
    }

    std::string str(size_t len)
    {
        if (p + len > end) throw std::runtime_error("msgpack: truncated");
        std::string s(reinterpret_cast<const char*>(p), len);
        p += len;
        return s;
    }

    void validateCount(uint32_t n) const
    {
        constexpr uint32_t MAX_ELEMENTS = 1 << 20;
        if (n > MAX_ELEMENTS || n > static_cast<uint32_t>(end - p))
            throw std::runtime_error("msgpack: element count exceeds remaining data");
    }

    json::Value readArray(uint32_t n)
    {
        validateCount(n);
        json::Value v;
        v.kind = json::Value::Kind::Array;
        v.children.reserve(n);
        for (uint32_t i = 0; i < n; ++i) v.children.push_back(read());
        return v;
    }

    json::Value readMap(uint32_t n)
    {
        validateCount(n);
        json::Value v;
        v.kind = json::Value::Kind::Object;
        for (uint32_t i = 0; i < n; ++i)
        {
            json::Value keyVal = read();
            std::string key = keyVal.kind == json::Value::Kind::String ? keyVal.scalar : std::to_string(i);
            v.fields[key] = read();
        }
        return v;
    }

    json::Value readString(uint32_t n)
    {
        json::Value v;
        v.kind = json::Value::Kind::String;
        v.scalar = str(n);
        return v;
    }

    json::Value readExt(uint32_t dataLen)
    {
        int8_t type = static_cast<int8_t>(u8());
        json::Value v;
        if (type == 10) // function reference
        {
            v.kind = json::Value::Kind::FuncRef;
            v.scalar = str(dataLen);
        }
        else
        {
            if (p + dataLen > end) throw std::runtime_error("msgpack: truncated");
            p += dataLen;
        }
        return v;
    }

    json::Value read()
    {
        uint8_t b = u8();
        json::Value v;

        if ((b & 0x80) == 0)
        {
            v.kind = json::Value::Kind::Number;
            v.scalar = std::to_string(b);
            return v;
        }
        if ((b & 0xE0) == 0xE0)
        {
            v.kind = json::Value::Kind::Number;
            v.scalar = std::to_string(static_cast<int8_t>(b));
            return v;
        }
        if ((b & 0xE0) == 0xA0) return readString(b & 0x1F);
        if ((b & 0xF0) == 0x80) return readMap(b & 0x0F);
        if ((b & 0xF0) == 0x90) return readArray(b & 0x0F);

        switch (b) {
        case 0xC0: v.kind = json::Value::Kind::Null; return v;
        case 0xC2: v.kind = json::Value::Kind::Bool; v.scalar = "false"; return v;
        case 0xC3: v.kind = json::Value::Kind::Bool; v.scalar = "true"; return v;

        case 0xC4: return readString(u8()); // bin8
        case 0xC5: return readString(u16()); // bin16
        case 0xC6: return readString(u32()); // bin32

        case 0xCA: {
            uint32_t bits = u32();
            float f; memcpy(&f, &bits, 4);
            char buf[32]; snprintf(buf, sizeof(buf), "%g", static_cast<double>(f));
            v.kind = json::Value::Kind::Number; v.scalar = buf; return v;
        }
        case 0xCB: {
            uint64_t bits = u64();
            double d; memcpy(&d, &bits, 8);
            char buf[32]; snprintf(buf, sizeof(buf), "%g", d);
            v.kind = json::Value::Kind::Number; v.scalar = buf; return v;
        }

        case 0xCC: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(u8()); return v;
        case 0xCD: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(u16()); return v;
        case 0xCE: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(u32()); return v;
        case 0xCF: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(u64()); return v;

        case 0xD0: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(static_cast<int8_t> (u8())); return v;
        case 0xD1: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(static_cast<int16_t>(u16())); return v;
        case 0xD2: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(static_cast<int32_t>(u32())); return v;
        case 0xD3: v.kind = json::Value::Kind::Number; v.scalar = std::to_string(static_cast<int64_t>(u64())); return v;

        case 0xD9: return readString(u8());
        case 0xDA: return readString(u16());
        case 0xDB: return readString(u32());

        case 0xDC: return readArray(u16());
        case 0xDD: return readArray(u32());

        case 0xDE: return readMap(u16());
        case 0xDF: return readMap(u32());

        case 0xC7: return readExt(u8());
        case 0xC8: return readExt(u16());
        case 0xC9: return readExt(u32());
        case 0xD4: return readExt(1);
        case 0xD5: return readExt(2);
        case 0xD6: return readExt(4);
        case 0xD7: return readExt(8);
        case 0xD8: return readExt(16);

        default:
            v.kind = json::Value::Kind::Null;
            return v;
        }
    }
};

}

inline json::Value decode(const char* data, uint32_t size)
{
    try
    {
        detail::Reader r{ reinterpret_cast<const uint8_t*>(data), reinterpret_cast<const uint8_t*>(data) + size };
        return r.read();
    }
    catch (...)
    {
        return {};
    }
}

}
