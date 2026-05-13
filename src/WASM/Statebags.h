#pragma once

#include "Imports.h"
#include "Types.h"
#include "Refs.h"
#include "Natives.h"

namespace fx
{

inline void setStateBagValue(const std::string& bagName, const std::string& key, const json::Value& value, bool replicated = true)
{
    auto encoded = fxw_internal::encode(value);
    invokeNative(HashString("SET_STATE_BAG_VALUE"), {NativeArg::ptr(bagName.c_str()), NativeArg::ptr(key.c_str()), NativeArg::ptr(encoded.data()), NativeArg(static_cast<int32_t>(encoded.size())), NativeArg(replicated ? 1 : 0)});
}

inline void setPlayerState(int serverId, const std::string& key, const json::Value& value, bool replicated = true)
{
    setStateBagValue("player:" + std::to_string(serverId), key, value, replicated);
}

inline void setEntityState(int netId, const std::string& key, const json::Value& value, bool replicated = true)
{
    setStateBagValue("entity:" + std::to_string(netId), key, value, replicated);
}

inline void setGlobalState(const std::string& key, const json::Value& value, bool replicated = true)
{
    setStateBagValue("global", key, value, replicated);
}

inline json::Value getStateBagValue(const std::string& bagName, const std::string& key)
{
    auto ctx = invokeNative(HashString("GET_STATE_BAG_VALUE"), {NativeArg::ptr(bagName.c_str()), NativeArg::ptr(key.c_str())}, 2);
    uintptr_t dataAddr = static_cast<uintptr_t>(ctx.args[0]);
    size_t size = static_cast<size_t>(ctx.args[1]);
    if (!dataAddr || size == 0) return json::makeNull();
    return fxw_internal::decode(reinterpret_cast<const void*>(dataAddr), static_cast<uint32_t>(size));
}

inline json::Value getPlayerState(int serverId, const std::string& key)
{
    return getStateBagValue("player:" + std::to_string(serverId), key);
}

inline json::Value getEntityState(int netId, const std::string& key)
{
    return getStateBagValue("entity:" + std::to_string(netId), key);
}

inline json::Value getGlobalState(const std::string& key)
{
    return getStateBagValue("global", key);
}

inline bool stateBagHasKey(const std::string& bagName, const std::string& key)
{
    auto ctx = invokeNative(HashString("STATE_BAG_HAS_KEY"), {NativeArg::ptr(bagName.c_str()), NativeArg::ptr(key.c_str())}, 1);
    return ctx.args[0] != 0;
}

inline std::vector<std::string> getStateBagKeys(const std::string& bagName)
{
    auto ctx = invokeNative(HashString("GET_STATE_BAG_KEYS"), {NativeArg::ptr(bagName.c_str())}, 2);
    uintptr_t dataAddr = static_cast<uintptr_t>(ctx.args[0]);
    size_t size = static_cast<size_t>(ctx.args[1]);
    if (!dataAddr || size == 0) return {};
    auto arr = fxw_internal::decode(reinterpret_cast<const void*>(dataAddr), static_cast<uint32_t>(size));
    std::vector<std::string> keys;
    for (size_t i = 0; i < arr.size(); i++)
        keys.push_back(arr.at(i).asStr());
    return keys;
}

inline int32_t addStateBagChangeHandler(const std::string& keyFilter, const std::string& bagFilter, StateBagChangeHandler handler)
{
    int32_t hostRef = detail::createRef([handler](const uint8_t* args, uint32_t argsSize) -> std::vector<uint8_t> {
        auto decoded = fxw_internal::decode(args, argsSize);
        fxw_internal::ensureArray(decoded);
        if (decoded.size() >= 5)
        {
            std::string bagName = decoded.at(0).asStr();
            std::string key = decoded.at(1).asStr();
            json::Value value = decoded.at(2);
            int source = decoded.at(3).asInt();
            bool replicated = decoded.at(4).asBool();
            handler(bagName, key, value, source, replicated);
        }
        return { 0x90 };
    });
    std::string cbRef = detail::canonicalizeRef(hostRef);
    if (cbRef.empty()) { detail::removeRef(hostRef); return -1; }
    const char* keyArg = keyFilter.empty() ? nullptr : keyFilter.c_str();
    const char* bagArg = bagFilter.empty() ? nullptr : bagFilter.c_str();
    auto ctx = invokeNative(HashString("ADD_STATE_BAG_CHANGE_HANDLER"), {NativeArg::ptr(keyArg), NativeArg::ptr(bagArg), NativeArg::ptr(cbRef.c_str())}, 1);
    return static_cast<int32_t>(ctx.args[0]);
}

inline void removeStateBagChangeHandler(int32_t cookie)
{
    invokeNative(HashString("REMOVE_STATE_BAG_CHANGE_HANDLER"), {NativeArg(cookie)});
}

}
