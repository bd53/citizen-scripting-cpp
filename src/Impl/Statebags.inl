namespace fx
{

inline void ResourceContext::setStateBagValue(const std::string& bagName, const std::string& key, const json::Value& value, bool replicated)
{
    auto encoded = msgpack::encode(value);
    invokeNative(HashString("SET_STATE_BAG_VALUE"), reinterpret_cast<uintptr_t>(bagName.c_str()), reinterpret_cast<uintptr_t>(key.c_str()), reinterpret_cast<uintptr_t>(encoded.data()), encoded.size(), uintptr_t(replicated ? 1u : 0u));
}

inline void ResourceContext::setPlayerState(int serverId, const std::string& key, const json::Value& value, bool replicated)
{
    setStateBagValue("player:" + std::to_string(serverId), key, value, replicated);
}

inline void ResourceContext::setEntityState(int netId, const std::string& key, const json::Value& value, bool replicated)
{
    setStateBagValue("entity:" + std::to_string(netId), key, value, replicated);
}

inline void ResourceContext::setGlobalState(const std::string& key, const json::Value& value, bool replicated)
{
    setStateBagValue("global", key, value, replicated);
}

inline json::Value ResourceContext::getStateBagValue(const std::string& bagName, const std::string& key)
{
    auto ctx = invokeNativeResult(HashString("GET_STATE_BAG_VALUE"), reinterpret_cast<uintptr_t>(bagName.c_str()), reinterpret_cast<uintptr_t>(key.c_str()));
    const char* data = reinterpret_cast<const char*>(ctx.arguments[0]);
    size_t size = static_cast<size_t>(ctx.arguments[1]);
    if (!data || size == 0) return json::makeNull();
    return msgpack::decode(data, static_cast<uint32_t>(size));
}

inline json::Value ResourceContext::getPlayerState(int serverId, const std::string& key)
{
    return getStateBagValue("player:" + std::to_string(serverId), key);
}

inline json::Value ResourceContext::getEntityState(int netId, const std::string& key)
{
    return getStateBagValue("entity:" + std::to_string(netId), key);
}

inline json::Value ResourceContext::getGlobalState(const std::string& key)
{
    return getStateBagValue("global", key);
}

inline bool ResourceContext::stateBagHasKey(const std::string& bagName, const std::string& key)
{
    auto ctx = invokeNativeResult(HashString("STATE_BAG_HAS_KEY"), reinterpret_cast<uintptr_t>(bagName.c_str()), reinterpret_cast<uintptr_t>(key.c_str()));
    return static_cast<bool>(ctx.arguments[0]);
}

inline std::vector<std::string> ResourceContext::getStateBagKeys(const std::string& bagName)
{
    auto ctx = invokeNativeResult(HashString("GET_STATE_BAG_KEYS"), reinterpret_cast<uintptr_t>(bagName.c_str()));
    const char* data = reinterpret_cast<const char*>(ctx.arguments[0]);
    size_t size = static_cast<size_t>(ctx.arguments[1]);
    if (!data || size == 0) return {};
    json::Value arr = msgpack::decode(data, static_cast<uint32_t>(size));
    std::vector<std::string> keys;
    for (size_t i = 0; i < arr.size(); i++)
        keys.push_back(arr.at(i).asStr());
    return keys;
}

inline int32_t ResourceContext::addStateBagChangeHandler(const std::string& keyFilter, const std::string& bagFilter, StateBagChangeHandler handler)
{
    if (!m_addRef) return -1;

    int32_t refIdx = m_addRef([handler](const char* argsSerialized, uint32_t argsSize) -> std::vector<char> {
        json::Value args = msgpack::decode(argsSerialized, argsSize);
        json::ensureArray(args);
        if (args.size() >= 5)
        {
            std::string bagName = args.at(0).asStr();
            std::string key = args.at(1).asStr();
            json::Value value = args.at(2);
            int source = args.at(3).asInt();
            bool replicated = args.at(4).asBool();
            handler(bagName, key, value, source, replicated);
        }
        return { static_cast<char>(0x90) }; // msgpack empty array
    });

    char* refString = nullptr;
    m_host->CanonicalizeRef(refIdx, m_runtime->GetInstanceId(), &refString);
    if (!refString) { if (m_removeRef) m_removeRef(refIdx); return -1; }
    std::string cbRef = refString;
    fwFree(refString);

    const char* keyArg = keyFilter.empty() ? nullptr : keyFilter.c_str();
    const char* bagArg = bagFilter.empty() ? nullptr : bagFilter.c_str();
    auto ctx = invokeNativeResult(HashString("ADD_STATE_BAG_CHANGE_HANDLER"), reinterpret_cast<uintptr_t>(keyArg), reinterpret_cast<uintptr_t>(bagArg), reinterpret_cast<uintptr_t>(cbRef.c_str()));
    int32_t cookie = static_cast<int32_t>(ctx.arguments[0]);
    m_stateBagHandlerRefs[cookie] = refIdx;
    return cookie;
}

inline void ResourceContext::removeStateBagChangeHandler(int32_t cookie)
{
    invokeNative(HashString("REMOVE_STATE_BAG_CHANGE_HANDLER"), static_cast<uintptr_t>(cookie));
    auto it = m_stateBagHandlerRefs.find(cookie);
    if (it != m_stateBagHandlerRefs.end())
    {
        if (m_removeRef) m_removeRef(it->second);
        m_stateBagHandlerRefs.erase(it);
    }
}

inline void ResourceContext::cleanupStateBagHandlers()
{
    for (auto& [cookie, refIdx] : m_stateBagHandlerRefs)
    {
        invokeNative(HashString("REMOVE_STATE_BAG_CHANGE_HANDLER"), static_cast<uintptr_t>(cookie));
        if (m_removeRef) m_removeRef(refIdx);
    }
    m_stateBagHandlerRefs.clear();
}

}
