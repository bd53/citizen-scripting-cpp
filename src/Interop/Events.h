#pragma once

#include "../Resource.h"
#include "../EventTraits.h"

namespace fx
{

template<typename F>
inline void on(const std::string& event, F&& handler)
{
    if (auto* ctx = detail::g_ctx)
    {
        if constexpr (std::is_convertible_v<std::decay_t<F>, EventHandler>)
            ctx->on(event, EventHandler(std::forward<F>(handler)));
        else
            ctx->on(event, detail::wrap_typed_handler(std::forward<F>(handler)));
    }
}

template<typename F>
inline void onNet(const std::string& event, F&& handler)
{
    if (auto* ctx = detail::g_ctx)
    {
        if constexpr (std::is_convertible_v<std::decay_t<F>, EventHandler>)
            ctx->onNet(event, EventHandler(std::forward<F>(handler)));
        else
            ctx->onNet(event, detail::wrap_typed_handler(std::forward<F>(handler)));
    }
}

inline void onTick(TickHandler handler)
{
    if (auto* ctx = detail::g_ctx)
        ctx->onTick(std::move(handler));
}

inline void onCommand(const std::string& command, CommandHandler handler)
{
    if (auto* ctx = detail::g_ctx)
        ctx->onCommand(command, std::move(handler));
}

inline void onStop(StopHandler handler)
{
    if (auto* ctx = detail::g_ctx)
        ctx->onStop(std::move(handler));
}

template<typename... TArgs>
inline void trace(const char* fmt, TArgs&&... args)
{
    if (auto* ctx = detail::g_ctx)
        ctx->trace(fmt, std::forward<TArgs>(args)...);
}

inline void traceStr(const std::string& msg)
{
    if (auto* ctx = detail::g_ctx)
        ctx->traceStr(msg);
}

template<typename... TArgs>
inline void emit(const std::string& event, TArgs&&... args)
{
    if (auto* ctx = detail::g_ctx)
        ctx->emit(event, {json::Value(std::forward<TArgs>(args))...});
}

template<typename... TArgs>
inline void emitNet(const std::string& event, int target, TArgs&&... args)
{
    if (auto* ctx = detail::g_ctx)
        ctx->emitNet(event, target, {json::Value(std::forward<TArgs>(args))...});
}

inline void cancelEvent()
{
    if (auto* ctx = detail::g_ctx)
        ctx->cancelEvent();
}

inline bool wasEventCanceled()
{
    if (auto* ctx = detail::g_ctx)
        return ctx->wasEventCanceled();
    return false;
}

}
