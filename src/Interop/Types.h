#pragma once

#include "../Json.h"

#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <coroutine>
#include <cstdint>

namespace fx
{

struct Vector3
{
    float x = 0.f, y = 0.f, z = 0.f;
};

class EventArgs
{
public:
    explicit EventArgs(json::Value arr) : m_arr(std::move(arr)) {}
    size_t size() const { return m_arr.size(); }
    template<typename T> T get(size_t i) const;
    std::string str (size_t i) const { return m_arr.at(i).asStr(); }
    int integer(size_t i) const { return m_arr.at(i).asInt(); }
    double number (size_t i) const { return m_arr.at(i).asNum(); }
    bool boolean(size_t i) const { return m_arr.at(i).asBool(); }
    bool isNull (size_t i) const { return m_arr.at(i).isNull(); }
    std::string funcRef(size_t i) const
    {
        const auto& v = m_arr.at(i);
        return v.kind == json::Value::Kind::FuncRef ? v.scalar : std::string{};
    }

private:
    json::Value m_arr;
};

template<> inline std::string EventArgs::get<std::string>(size_t i) const { return str(i); }
template<> inline int EventArgs::get<int>(size_t i) const { return integer(i); }
template<> inline double EventArgs::get<double>(size_t i) const { return number(i); }
template<> inline float EventArgs::get<float>(size_t i) const { return static_cast<float>(number(i)); }
template<> inline bool EventArgs::get<bool>(size_t i) const { return boolean(i); }

using EventHandler = std::function<void(const std::string& source, EventArgs)>;
using TickHandler = std::function<void()>;
using CommandHandler = std::function<void(const std::string& source, const std::vector<std::string>& args)>;
using RefCallback = std::function<std::vector<char>(const char* argsSerialized, uint32_t argsSize)>;
using AddRefFn = std::function<int32_t(RefCallback)>;
using ExportHandler = std::function<json::Value(EventArgs)>;
using StopHandler = std::function<void()>;
using RemoveRefFn = std::function<void(int32_t)>;
using ScheduleBookmarkFn = std::function<void(uint64_t, int64_t)>;
using StateBagChangeHandler = std::function<void(const std::string& bagName, const std::string& key, const json::Value& value, int source, bool replicated)>;

struct BookmarkPromise;
using BookmarkHandle = std::coroutine_handle<BookmarkPromise>;

struct TimerEntry
{
    int32_t id;
    std::chrono::steady_clock::time_point nextFire;
    uint32_t intervalMs;
    std::function<void()> callback;
};

}
