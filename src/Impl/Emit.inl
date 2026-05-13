namespace fx
{

inline void ResourceContext::trace(const char* fmt, ...)
{
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    m_host->ScriptTrace(buf);
    fprintf(stderr, "[script:%s] %s", m_name.c_str(), buf);
}

inline void ResourceContext::emit(const std::string& event, std::initializer_list<json::Value> args)
{
    json::Value arr;
    arr.kind = json::Value::Kind::Array;
    arr.children.assign(args.begin(), args.end());
    auto payload = msgpack::encode(arr);
    invokeNative(HashString("TRIGGER_EVENT_INTERNAL"), reinterpret_cast<uintptr_t>(event.c_str()), reinterpret_cast<uintptr_t>(payload.data()), payload.size());
}

inline void ResourceContext::emitNet(const std::string& event, int target, std::initializer_list<json::Value> args)
{
    json::Value arr;
    arr.kind = json::Value::Kind::Array;
    arr.children.assign(args.begin(), args.end());
    auto payload = msgpack::encode(arr);
    std::string targetStr = std::to_string(target);
    invokeNative(HashString("TRIGGER_CLIENT_EVENT_INTERNAL"), reinterpret_cast<uintptr_t>(event.c_str()), reinterpret_cast<uintptr_t>(targetStr.c_str()), reinterpret_cast<uintptr_t>(payload.data()), payload.size());
}

inline void ResourceContext::cancelEvent()
{
    invokeNative(HashString("CANCEL_EVENT"));
}

inline bool ResourceContext::wasEventCanceled()
{
    auto ctx = invokeNativeResult(HashString("WAS_EVENT_CANCELED"));
    return static_cast<bool>(ctx.arguments[0]);
}

}
