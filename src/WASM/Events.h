#pragma once

#include "Imports.h"
#include "Types.h"
#include "Context.h"

namespace fx
{

inline void on(const std::string& event, EventHandler h)
{
    if (auto* c = fxw_internal::currentContext())
    {
        bool first = c->events.find(event) == c->events.end() || c->events[event].empty();
        c->events[event].push_back(std::move(h));
        if (first)
        {
            NativeCtx ctx{};
            ctx.hash = HashString("REGISTER_RESOURCE_AS_EVENT_HANDLER");
            ctx.numArgs = 1;
            ctx.args[0] = reinterpret_cast<uint64_t>(event.c_str());
            ctx.ptrMask = 1;
            __fxcpp_invoke_native(reinterpret_cast<uint32_t>(&ctx));
        }
    }
}

inline void onNet(const std::string& event, EventHandler h)
{
    on(event, std::move(h));
}

template<typename... TArgs>
inline void emit(const std::string& event, TArgs&&... vals)
{
    fxw_internal::Writer w;
    w.arrayHeader(static_cast<uint32_t>(sizeof...(vals)));
    (fxw_internal::encodeOne(w, std::forward<TArgs>(vals)), ...);
    __fxcpp_emit_event(event.c_str(), static_cast<uint32_t>(event.size()), w.buf.data(), static_cast<uint32_t>(w.buf.size()));
}

template<typename... TArgs>
inline void emitNet(const std::string& event, int target, TArgs&&... vals)
{
    fxw_internal::Writer w;
    w.arrayHeader(static_cast<uint32_t>(sizeof...(vals)));
    (fxw_internal::encodeOne(w, std::forward<TArgs>(vals)), ...);
    __fxcpp_emit_net_event(event.c_str(), static_cast<uint32_t>(event.size()), target, w.buf.data(), static_cast<uint32_t>(w.buf.size()));
}

inline void cancelEvent() { __fxcpp_cancel_event(); }
inline bool wasEventCanceled() { return __fxcpp_was_event_canceled() != 0; }

}
