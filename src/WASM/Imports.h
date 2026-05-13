#pragma once

#include <cstdint>
#include <string_view>

inline constexpr uint32_t HashString(std::string_view str)
{
    uint32_t hash = 0;
    for (char c : str)
    {
        hash += (c >= 'A' && c <= 'Z') ? static_cast<uint32_t>(c | 0x20) : static_cast<uint32_t>(c);
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

extern "C"
{
    __attribute__((import_module("fxcpp"), import_name("trace")))
    void __fxcpp_trace(const char* ptr, uint32_t len);

    __attribute__((import_module("fxcpp"), import_name("invoke_native")))
    void __fxcpp_invoke_native(uint32_t ctx_ptr);

    __attribute__((import_module("fxcpp"), import_name("copy_string_result")))
    int32_t __fxcpp_copy_string_result(uint32_t ctx_ptr, int32_t result_idx, char* buf, int32_t buf_max);

    __attribute__((import_module("fxcpp"), import_name("emit_event")))
    void __fxcpp_emit_event(const char* name, uint32_t name_len, const uint8_t* args, uint32_t args_len);

    __attribute__((import_module("fxcpp"), import_name("emit_net_event")))
    void __fxcpp_emit_net_event(const char* name, uint32_t name_len, int32_t target, const uint8_t* args, uint32_t args_len);

    __attribute__((import_module("fxcpp"), import_name("cancel_event")))
    void __fxcpp_cancel_event();

    __attribute__((import_module("fxcpp"), import_name("was_event_canceled")))
    int32_t __fxcpp_was_event_canceled();

    __attribute__((import_module("fxcpp"), import_name("get_resource_metadata")))
    int32_t __fxcpp_get_resource_metadata(const char* key, uint32_t key_len, int32_t index, char* buf, int32_t buf_max);

    __attribute__((import_module("fxcpp"), import_name("get_num_resource_metadata")))
    int32_t __fxcpp_get_num_resource_metadata(const char* key, uint32_t key_len);

    __attribute__((import_module("fxcpp"), import_name("create_ref")))
    int32_t __fxcpp_create_ref(int32_t callback_id);

    __attribute__((import_module("fxcpp"), import_name("canonicalize_ref")))
    int32_t __fxcpp_canonicalize_ref(int32_t ref_idx, char* buf, int32_t buf_max);

    __attribute__((import_module("fxcpp"), import_name("remove_ref")))
    void __fxcpp_remove_ref(int32_t ref_idx);

    __attribute__((import_module("fxcpp"), import_name("invoke_function_reference")))
    void __fxcpp_invoke_function_reference(const char* ref_str, uint32_t ref_len, const char* args, uint32_t args_len, void* out);

    __attribute__((import_module("fxcpp"), import_name("get_instance_id")))
    int32_t __fxcpp_get_instance_id();
}
