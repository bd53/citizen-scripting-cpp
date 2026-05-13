namespace fx
{

inline void ResourceContext::trace(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int len = vsnprintf(nullptr, 0, fmt, ap);
    va_end(ap);
    if (len <= 0) { va_end(ap2); return; }
    std::string buf(static_cast<size_t>(len), '\0');
    vsnprintf(buf.data(), buf.size() + 1, fmt, ap2);
    va_end(ap2);
    m_host->ScriptTrace(buf.data());
    fprintf(stderr, "[script:%s] %s", m_name.c_str(), buf.c_str());
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
