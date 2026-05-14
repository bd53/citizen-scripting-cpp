#pragma once

#include "Types.h"

#include <chrono>
#include <unordered_map>

namespace fxw_internal
{

struct TimerEntry
{
    int32_t id;
    std::chrono::steady_clock::time_point nextFire;
    uint32_t intervalMs;
    std::function<void()> callback;
};

struct Context
{
    std::vector<fx::TickHandler> ticks;
    std::vector<fx::StopHandler> stops;
    std::unordered_map<std::string, std::vector<fx::EventHandler>> events;
    std::unordered_map<std::string, std::vector<fx::CommandHandler>> commands;
    std::unordered_map<int32_t, TimerEntry> timers;
    int32_t nextTimerId = 1;

    void dispatchTick()
    {
        auto now = std::chrono::steady_clock::now();
        std::vector<int32_t> expired;
        for (auto& [id, t] : timers)
            if (now >= t.nextFire) expired.push_back(id);
        for (auto id : expired)
        {
            auto it = timers.find(id);
            if (it == timers.end()) continue;
            auto cb = it->second.callback;
            if (it->second.intervalMs > 0)
            {
                auto next = it->second.nextFire + std::chrono::milliseconds(it->second.intervalMs);
                it->second.nextFire = (next > now) ? next : now;
            }
            else
                timers.erase(it);
            cb();
        }
        for (auto& h : ticks) h();
    }

    void dispatchStop()
    { for (auto& h : stops) h(); }

    void dispatchEvent(const char* name, uint32_t nameLen, const uint8_t* args, uint32_t argsLen, const char* src, uint32_t srcLen)
    {
        std::string key(name, nameLen);
        auto it = events.find(key);
        if (it == events.end()) return;

        Value decoded = decode(args, argsLen);
        ensureArray(decoded);
        fx::EventArgs ea(std::move(decoded));
        std::string srcStr(src, srcLen);
        for (auto& h : it->second) h(srcStr, ea);
    }
};

inline Context*& currentContext() { static Context* p = nullptr; return p; }

inline std::unordered_map<int32_t, RefCallbackFn>& refCallbacks()
{
    static std::unordered_map<int32_t, RefCallbackFn> s_map;
    return s_map;
}

inline std::unordered_map<int32_t, int32_t>& refCounts()
{
    static std::unordered_map<int32_t, int32_t> s_map;
    return s_map;
}

inline int32_t& nextCallbackId()
{
    static int32_t s_id = 1;
    return s_id;
}

}
