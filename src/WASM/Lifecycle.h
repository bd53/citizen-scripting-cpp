#pragma once

#include "Imports.h"
#include "Context.h"

namespace fx
{

template<typename... TArgs>
inline void trace(const char* fmt, TArgs&&... args)
{
    int len = snprintf(nullptr, 0, fmt, std::forward<TArgs>(args)...);
    if (len <= 0) return;
    std::string buf(static_cast<size_t>(len), '\0');
    snprintf(buf.data(), buf.size() + 1, fmt, std::forward<TArgs>(args)...);
    __fxcpp_trace(buf.data(), static_cast<uint32_t>(buf.size()));
}

inline void traceStr(const std::string& msg)
{
    if (!msg.empty())
        __fxcpp_trace(msg.data(), static_cast<uint32_t>(msg.size()));
}

inline void onTick(TickHandler h)
{
    if (auto* c = fxw_internal::currentContext())
        c->ticks.push_back(std::move(h));
}

inline void onStop(StopHandler h)
{
    if (auto* c = fxw_internal::currentContext())
        c->stops.push_back(std::move(h));
}

}
