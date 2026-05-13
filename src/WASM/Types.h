#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace fxw_internal
{

struct Value
{
    enum class Kind { Null, Bool, Number, String, Array, FuncRef } kind = Kind::Null;
    std::string scalar;
    std::vector<Value> children;
    Value() = default;
    Value(Value&&) noexcept = default;
    Value& operator=(Value&&) noexcept = default;
    Value(const Value&) = default;
    Value& operator=(const Value&) = default;
    Value(int v) : kind(Kind::Number), scalar(std::to_string(v)) {}
    Value(int64_t v) : kind(Kind::Number), scalar(std::to_string(v)) {}
    Value(double v) : kind(Kind::Number) { char buf[32]; snprintf(buf, sizeof(buf), "%g", v); scalar = buf; }
    Value(float v) : kind(Kind::Number) { char buf[32]; snprintf(buf, sizeof(buf), "%g", static_cast<double>(v)); scalar = buf; }
    Value(bool b) : kind(Kind::Bool), scalar(b ? "true" : "false") {}
    Value(const char* s) : kind(s ? Kind::String : Kind::Null), scalar(s ? s : "") {}
    Value(std::string s) : kind(Kind::String), scalar(std::move(s)) {}
    std::string asStr(std::string_view def = "") const
    { return kind == Kind::String ? scalar : std::string(def); }
    double asNum(double def = 0.0) const
    { return kind == Kind::Number ? std::strtod(scalar.c_str(), nullptr) : def; }
    int asInt(int def = 0) const { return static_cast<int>(asNum(def)); }
    float asFloat(float def = 0.f) const { return static_cast<float>(asNum(def)); }
    bool asBool(bool def = false) const
    { return kind == Kind::Bool ? scalar == "true" : def; }
    bool isNull() const { return kind == Kind::Null; }
    size_t size() const { return children.size(); }
    const Value& at(size_t i) const
    { static const Value nil; return i < children.size() ? children[i] : nil; }
};

struct Reader
{
    const uint8_t* p; const uint8_t* end;
    uint8_t u8() { return p < end ? *p++ : 0; }
    uint16_t u16() { uint8_t a=u8(),b=u8(); return (uint16_t)(a<<8|b); }
    uint32_t u32() { uint16_t a=u16(),b=u16(); return (uint32_t)(a<<16|b); }
    uint64_t u64() { uint32_t a=u32(),b=u32(); return (uint64_t)a<<32|b; }
    std::string str(uint32_t n)
    { if(p+n>end) n=(uint32_t)(end-p);
      std::string s((const char*)p,n); p+=n; return s; }
    Value read(int d=0)
    {
        if (d > 64 || p >= end) return {};
        uint8_t b = u8(); Value v;
        if ((b&0x80)==0) { v.kind=Value::Kind::Number; v.scalar=std::to_string(b); return v; }
        if ((b&0xE0)==0xE0){ v.kind=Value::Kind::Number; v.scalar=std::to_string((int8_t)b); return v; }
        if ((b&0xE0)==0xA0){ v.kind=Value::Kind::String; v.scalar=str(b&0x1F); return v; }
        if ((b&0xF0)==0x90){
            v.kind=Value::Kind::Array;
            uint32_t n=b&0x0F;
            for(uint32_t i=0;i<n;++i) v.children.push_back(read(d+1));
            return v;
        }
        if ((b&0xF0)==0x80){
            uint32_t n=b&0x0F;
            if (n == 1) {
                auto key = read(d+1);
                auto val = read(d+1);
                if (key.kind == Value::Kind::String && key.scalar == "__cfx_functionReference") {
                    v.kind = Value::Kind::FuncRef;
                    v.scalar = val.asStr();
                    return v;
                }
                return {};
            }
            for(uint32_t i=0;i<n;++i) { read(d+1); read(d+1); }
            return {};
        }
        switch(b){
        case 0xC0: return {};
        case 0xC2: v.kind=Value::Kind::Bool; v.scalar="false"; return v;
        case 0xC3: v.kind=Value::Kind::Bool; v.scalar="true"; return v;
        case 0xC4: case 0xD9: { v.kind=Value::Kind::String; v.scalar=str(u8()); return v; }
        case 0xC5: case 0xDA: { v.kind=Value::Kind::String; v.scalar=str(u16()); return v; }
        case 0xC6: case 0xDB: { v.kind=Value::Kind::String; v.scalar=str(u32()); return v; }
        case 0xCC:{ v.kind=Value::Kind::Number; v.scalar=std::to_string(u8()); return v; }
        case 0xCD:{ v.kind=Value::Kind::Number; v.scalar=std::to_string(u16()); return v; }
        case 0xCE:{ v.kind=Value::Kind::Number; v.scalar=std::to_string(u32()); return v; }
        case 0xCF:{ v.kind=Value::Kind::Number; v.scalar=std::to_string(u64()); return v; }
        case 0xD0:{ v.kind=Value::Kind::Number; v.scalar=std::to_string((int8_t) u8()); return v; }
        case 0xD1:{ v.kind=Value::Kind::Number; v.scalar=std::to_string((int16_t)u16()); return v; }
        case 0xD2:{ v.kind=Value::Kind::Number; v.scalar=std::to_string((int32_t)u32()); return v; }
        case 0xD3:{ v.kind=Value::Kind::Number; v.scalar=std::to_string((int64_t)u64()); return v; }
        case 0xCA:{ uint32_t bits=u32(); float f; memcpy(&f,&bits,4);
            char buf[32]; snprintf(buf,sizeof(buf),"%g",(double)f);
            v.kind=Value::Kind::Number; v.scalar=buf; return v; }
        case 0xCB:{ uint64_t bits=u64(); double d; memcpy(&d,&bits,8);
            char buf[32]; snprintf(buf,sizeof(buf),"%g",d);
            v.kind=Value::Kind::Number; v.scalar=buf; return v; }
        case 0xDC:{ v.kind=Value::Kind::Array; uint32_t n=u16();
            for(uint32_t i=0;i<n;++i) v.children.push_back(read(d+1)); return v; }
        case 0xDD:{ v.kind=Value::Kind::Array; uint32_t n=u32();
            for(uint32_t i=0;i<n;++i) v.children.push_back(read(d+1)); return v; }
        // ext8/ext16/ext32
        case 0xC7:{ uint32_t n=u8(); uint8_t t=u8();
            if(t==10){ v.kind=Value::Kind::FuncRef; v.scalar=str(n); return v; }
            p+=n; return {}; }
        case 0xC8:{ uint32_t n=u16(); uint8_t t=u8();
            if(t==10){ v.kind=Value::Kind::FuncRef; v.scalar=str(n); return v; }
            p+=n; return {}; }
        case 0xC9:{ uint32_t n=u32(); uint8_t t=u8();
            if(t==10){ v.kind=Value::Kind::FuncRef; v.scalar=str(n); return v; }
            p+=n; return {}; }
        // map16/map32
        case 0xDE: { uint32_t n=u16(); for(uint32_t i=0;i<n;++i){ read(d+1); read(d+1); } return {}; }
        case 0xDF: { uint32_t n=u32(); for(uint32_t i=0;i<n;++i){ read(d+1); read(d+1); } return {}; }
        default: return {};
        }
    }
};

inline Value decode(const void* data, uint32_t size)
{
    Reader r{(const uint8_t*)data, (const uint8_t*)data + size};
    return r.read();
}

inline void ensureArray(Value& v)
{
    if (v.kind != Value::Kind::Array) {
        Value arr; arr.kind = Value::Kind::Array;
        arr.children.push_back(std::move(v));
        v = std::move(arr);
    }
}

struct Writer
{
    std::vector<uint8_t> buf;
    void pu8 (uint8_t v) { buf.push_back(v); }
    void pu16(uint16_t v) { buf.push_back(v>>8); buf.push_back(v&0xFF); }
    void pu32(uint32_t v) { pu16(v>>16); pu16(v&0xFFFF); }
    void str(std::string_view s)
    {
        uint32_t n = (uint32_t)s.size();
        if (n<=31) pu8(0xA0|(uint8_t)n);
        else if (n<=255) { pu8(0xD9); pu8((uint8_t)n); }
        else { pu8(0xDA); pu16((uint16_t)n); }
        buf.insert(buf.end(), s.begin(), s.end());
    }
    void arrayHeader(uint32_t n)
    {
        if (n<=15) pu8(0x90|(uint8_t)n);
        else { pu8(0xDC); pu16((uint16_t)n); }
    }
    void mapHeader(uint32_t n)
    {
        if (n<=15) pu8(0x80|(uint8_t)n);
        else { pu8(0xDE); pu16((uint16_t)n); }
    }
    void encNull() { pu8(0xC0); }
    void encBool(bool v) { pu8(v ? 0xC3 : 0xC2); }
    void encInt(int64_t v)
    {
        if (v>=0&&v<=127) { pu8((uint8_t)v); }
        else if(v<0&&v>=-32) { pu8((uint8_t)(int8_t)v); }
        else if(v>=-128&&v<=127) { pu8(0xD0); pu8((uint8_t)(int8_t)v); }
        else if(v>=-32768&&v<=32767){ pu8(0xD1); pu16((uint16_t)(int16_t)v); }
        else if(v>=-2147483648LL&&v<=2147483647LL){ pu8(0xD2); pu32((uint32_t)(int32_t)v); }
        else { pu8(0xD3); pu32((uint32_t)((uint64_t)v>>32)); pu32((uint32_t)v); }
    }
    void encDouble(double d)
    {
        uint64_t bits; memcpy(&bits,&d,8);
        pu8(0xCB);
        for(int i=7;i>=0;--i) pu8((bits>>(i*8))&0xFF);
    }
    void encValue(const Value& v)
    {
        switch (v.kind)
        {
        case Value::Kind::Null: encNull(); break;
        case Value::Kind::Bool: encBool(v.scalar == "true"); break;
        case Value::Kind::Number:
        {
            char* end = nullptr;
            long long iv = strtoll(v.scalar.c_str(), &end, 10);
            if (end && *end == '\0') encInt(iv);
            else encDouble(strtod(v.scalar.c_str(), nullptr));
            break;
        }
        case Value::Kind::String: str(v.scalar); break;
        case Value::Kind::FuncRef:
        {
            uint32_t n = (uint32_t)v.scalar.size();
            if (n<=255) { pu8(0xC7); pu8((uint8_t)n); }
            else if (n<=65535) { pu8(0xC8); pu16((uint16_t)n); }
            else { pu8(0xC9); pu32(n); }
            pu8(10); // function reference ext type
            buf.insert(buf.end(), v.scalar.begin(), v.scalar.end());
            break;
        }
        case Value::Kind::Array:
            arrayHeader(static_cast<uint32_t>(v.children.size()));
            for (auto& c : v.children) encValue(c);
            break;
        }
    }
};

inline std::vector<uint8_t> encode(const Value& v)
{
    Writer w;
    w.encValue(v);
    return std::move(w.buf);
}

template<typename T>
inline void encodeOne(Writer& w, T&& v)
{
    using D = std::decay_t<T>;
    if constexpr (std::is_same_v<D,std::string> || std::is_same_v<D,std::string_view> || std::is_same_v<D,const char*> || std::is_same_v<D,char*>)
        w.str(std::string_view(v));
    else if constexpr (std::is_same_v<D,bool>) w.encBool(v);
    else if constexpr (std::is_same_v<D, Value>) w.encValue(v);
    else if constexpr (std::is_floating_point_v<D>) w.encDouble((double)v);
    else if constexpr (std::is_integral_v<D>) w.encInt((int64_t)v);
    else w.encNull();
}

using RefCallbackFn = std::function<std::vector<uint8_t>(const uint8_t*, uint32_t)>;

}

namespace fx::json
{
    using Value = fxw_internal::Value;
    inline Value makeNull() { return {}; }
    inline void ensureArray(Value& v) { fxw_internal::ensureArray(v); }
}

namespace fx
{

struct NativeCtx
{
    uint64_t hash;
    uint32_t numArgs;
    uint32_t numResults;
    uint64_t args[32];
    uint32_t ptrMask;
    uint32_t resultPtrMask;
};
static_assert(sizeof(NativeCtx) == 280);

struct NativeArg
{
    uint64_t value = 0;
    bool isPtr = false;
    NativeArg() = default;
    NativeArg(int32_t v) : value((uint64_t)(uint32_t)v), isPtr(false) {}
    NativeArg(uint32_t v) : value((uint64_t)v), isPtr(false) {}
    NativeArg(int64_t v) : value((uint64_t)v), isPtr(false) {}
    NativeArg(uint64_t v) : value(v), isPtr(false) {}
    NativeArg(bool v) : value(v?1:0), isPtr(false) {}
    NativeArg(float v) { uint32_t b; memcpy(&b,&v,4); value=b; isPtr=false; }
    NativeArg(double v) { uint64_t b; memcpy(&b,&v,8); value=b; isPtr=false; }
    static NativeArg ptr(const void* p)
    { NativeArg a; a.value=(uint64_t)(uintptr_t)p; a.isPtr=true; return a; }
    static NativeArg ptr(void* p) { return ptr((const void*)p); }
};

struct Vector3
{
    float x = 0.f, y = 0.f, z = 0.f;
};

class EventArgs
{
public:
    explicit EventArgs(fxw_internal::Value arr) : m_arr(std::move(arr)) {}

    size_t size() const { return m_arr.size(); }
    std::string str(size_t i) const { return m_arr.at(i).asStr(); }
    int integer(size_t i) const { return m_arr.at(i).asInt(); }
    double number(size_t i) const { return m_arr.at(i).asNum(); }
    float floating(size_t i) const { return m_arr.at(i).asFloat(); }
    bool boolean(size_t i) const { return m_arr.at(i).asBool(); }
    bool isNull(size_t i) const { return m_arr.at(i).isNull(); }
    std::string funcRef(size_t i) const
    {
        const auto& v = m_arr.at(i);
        return v.kind == fxw_internal::Value::Kind::FuncRef ? v.scalar : std::string{};
    }

    template<typename T> T get(size_t i) const;

private:
    fxw_internal::Value m_arr;
};

template<> inline std::string EventArgs::get<std::string>(size_t i) const { return str(i); }
template<> inline int EventArgs::get<int> (size_t i) const { return integer(i); }
template<> inline double EventArgs::get<double> (size_t i) const { return number(i); }
template<> inline float EventArgs::get<float> (size_t i) const { return floating(i); }
template<> inline bool EventArgs::get<bool> (size_t i) const { return boolean(i); }

using EventHandler = std::function<void(const std::string& source, const EventArgs&)>;
using TickHandler = std::function<void()>;
using StopHandler = std::function<void()>;
using CommandHandler = std::function<void(const std::string& source, const std::vector<std::string>& args)>;
using ExportHandler = std::function<json::Value(const EventArgs&)>;
using StateBagChangeHandler = std::function<void(const std::string& bagName, const std::string& key, const json::Value& value, int source, bool replicated)>;

}
