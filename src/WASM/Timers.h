#pragma once

#include "Context.h"

namespace fx
{

inline int32_t setTimeout(uint32_t ms, std::function<void()> cb)
{
    auto* c = fxw_internal::currentContext();
    if (!c) return -1;
    int32_t id = c->nextTimerId++;
    auto fire = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    c->timers[id] = { id, fire, 0, std::move(cb) };
    return id;
}

inline int32_t setInterval(uint32_t ms, std::function<void()> cb)
{
    auto* c = fxw_internal::currentContext();
    if (!c) return -1;
    int32_t id = c->nextTimerId++;
    auto fire = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    c->timers[id] = { id, fire, ms, std::move(cb) };
    return id;
}

inline void clearTimer(int32_t id)
{
    auto* c = fxw_internal::currentContext();
    if (c) c->timers.erase(id);
}

}
