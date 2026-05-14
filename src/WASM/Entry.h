#pragma once

#include "Context.h"
#include "Refs.h"

#define FXCPP_WASM_EXPORT extern "C" __attribute__((visibility("default")))

FXCPP_WASM_EXPORT void* fxcpp_alloc(uint32_t size) { return malloc(size); }
FXCPP_WASM_EXPORT void fxcpp_free(void* ptr, uint32_t) { free(ptr); }

FXCPP_WASM_EXPORT void fxcpp_tick()
{
    if (auto* c = fxw_internal::currentContext()) c->dispatchTick();
}

FXCPP_WASM_EXPORT void fxcpp_on_event(const char* name, uint32_t nameLen, const uint8_t* args, uint32_t argsLen, const char* src, uint32_t srcLen)
{
    if (auto* c = fxw_internal::currentContext())
        c->dispatchEvent(name, nameLen, args, argsLen, src, srcLen);
}

FXCPP_WASM_EXPORT void fxcpp_on_stop()
{
    if (auto* c = fxw_internal::currentContext()) c->dispatchStop();
}

FXCPP_WASM_EXPORT int32_t fxcpp_invoke_ref(int32_t callback_id, const uint8_t* args_ptr, uint32_t args_len, uint8_t* result_ptr, uint32_t result_max)
{
    auto& callbacks = fxw_internal::refCallbacks();
    auto it = callbacks.find(callback_id);
    if (it == callbacks.end()) return 0;
    auto result = it->second(args_ptr, args_len);
    uint32_t copy = std::min<uint32_t>(static_cast<uint32_t>(result.size()), result_max);
    if (copy > 0 && result_ptr)
        memcpy(result_ptr, result.data(), copy);
    return static_cast<int32_t>(result.size());
}

FXCPP_WASM_EXPORT int32_t fxcpp_duplicate_ref(int32_t callback_id)
{
    auto& counts = fxw_internal::refCounts();
    auto it = counts.find(callback_id);
    if (it != counts.end())
        ++it->second;
    return callback_id;
}

FXCPP_WASM_EXPORT void fxcpp_remove_ref(int32_t callback_id)
{
    auto& counts = fxw_internal::refCounts();
    auto it = counts.find(callback_id);
    if (it == counts.end()) return;
    if (--it->second <= 0)
    {
        counts.erase(it);
        fxw_internal::refCallbacks().erase(callback_id);
    }
}

#define FXCPP_WASM_ENTRY \
    static void _fxcpp_wasm_body(); \
    FXCPP_WASM_EXPORT void fxcpp_init() \
    { \
        static fxw_internal::Context s_ctx; \
        fxw_internal::currentContext() = &s_ctx; \
        _fxcpp_wasm_body(); \
    } \
    static void _fxcpp_wasm_body()

#define Server FXCPP_WASM_ENTRY
