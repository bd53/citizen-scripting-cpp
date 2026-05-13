#pragma once

#include "Imports.h"
#include "Types.h"
#include "Natives.h"

namespace fx
{

inline std::string getResourceMetadata(const std::string& key, int index = 0)
{
    int32_t len = __fxcpp_get_resource_metadata(key.c_str(), static_cast<uint32_t>(key.size()), index, nullptr, 0);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    __fxcpp_get_resource_metadata(key.c_str(), static_cast<uint32_t>(key.size()), index, out.data(), len + 1);
    return out;
}

inline int getNumResourceMetadata(const std::string& key)
{
    return __fxcpp_get_num_resource_metadata(key.c_str(), static_cast<uint32_t>(key.size()));
}

inline std::string getCurrentResourceName()
{
    auto ctx = invokeNative(HashString("GET_CURRENT_RESOURCE_NAME"), {}, 1, 0x1);
    return getStringResult(ctx, 0);
}

inline std::string getInvokingResource()
{
    auto ctx = invokeNative(HashString("GET_INVOKING_RESOURCE"), {}, 1, 0x1);
    return getStringResult(ctx, 0);
}

inline std::string getResourceState(const std::string& resource)
{
    auto ctx = invokeNative(HashString("GET_RESOURCE_STATE"), {NativeArg::ptr(resource.c_str())}, 1, 0x1);
    return getStringResult(ctx, 0);
}

inline int getNumResources()
{
    auto ctx = invokeNative(HashString("GET_NUM_RESOURCES"), {}, 1);
    return static_cast<int>(ctx.args[0]);
}

inline std::string getResourceByIndex(int index)
{
    auto ctx = invokeNative(HashString("GET_RESOURCE_BY_FIND_INDEX"), {NativeArg(static_cast<int32_t>(index))}, 1, 0x1);
    return getStringResult(ctx, 0);
}

inline std::vector<std::string> getResources()
{
    std::vector<std::string> result;
    int num = getNumResources();
    for (int i = 0; i < num; i++)
    {
        std::string name = getResourceByIndex(i);
        if (!name.empty()) result.push_back(std::move(name));
    }
    return result;
}

}
