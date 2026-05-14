#pragma once

#include "Imports.h"
#include "Context.h"

namespace fx::detail
{

inline int32_t createRef(fxw_internal::RefCallbackFn cb)
{
    int32_t id = fxw_internal::nextCallbackId()++;
    fxw_internal::refCallbacks()[id] = std::move(cb);
    fxw_internal::refCounts()[id] = 1;
    int32_t hostRef = __fxcpp_create_ref(id);
    return hostRef;
}

inline std::string canonicalizeRef(int32_t refIdx)
{
    int32_t len = __fxcpp_canonicalize_ref(refIdx, nullptr, 0);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    __fxcpp_canonicalize_ref(refIdx, out.data(), len + 1);
    return out;
}

inline void removeRef(int32_t refIdx)
{
    __fxcpp_remove_ref(refIdx);
}

inline std::vector<uint8_t> invokeFunctionReference(const std::string& ref, const uint8_t* args, uint32_t argsLen)
{
    struct { uint32_t ptr; uint32_t len; } out{};
    __fxcpp_invoke_function_reference(ref.c_str(), static_cast<uint32_t>(ref.size()), reinterpret_cast<const char*>(args), argsLen, &out);
    if (!out.ptr || !out.len) return {};
    auto* data = reinterpret_cast<uint8_t*>(out.ptr);
    std::vector<uint8_t> result(data, data + out.len);
    free(data);
    return result;
}

}
