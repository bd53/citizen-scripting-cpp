#pragma once

#include "Interop/Types.h"
#include "Interop/MsgPackSerializer.h"
#include "Interop/MsgPackDeserializer.h"
#include "../include/fxScripting.h"

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>

#define FXCPP_RESOURCE_EXPORT __attribute__((visibility("default")))

namespace fx
{

class ResourceContext
{
public:
    ResourceContext(IScriptHost* host, IScriptRuntime* runtime, std::string name, IScriptRuntimeHandler* handler = nullptr, AddRefFn addRefFn = nullptr, RemoveRefFn removeRefFn = nullptr, ScheduleBookmarkFn scheduleBookmarkFn = nullptr) : m_host(host), m_runtime(runtime), m_name(std::move(name)), m_handler(fx::OMPtr<IScriptRuntimeHandler>(handler)), m_addRef(std::move(addRefFn)), m_removeRef(std::move(removeRefFn)), m_scheduleBookmark(std::move(scheduleBookmarkFn))
    {
        fx::OMPtr<IScriptHost> h(host);
        h.As(&m_metadataHost);
        h.As(&m_manifestHost);
    }

    // Events
    void on(const std::string& event, EventHandler h);
    void onNet(const std::string& event, EventHandler h);
    void onTick(TickHandler h);
    void onCommand(const std::string& command, CommandHandler h);

    // Lifecycle
    void onStop(StopHandler h);

    // Async / Threads
    void createThread(BookmarkHandle handle, std::shared_ptr<void> prevent_destruct = {});
    void resumeBookmarks(uint64_t* bookmarks, int32_t numBookmarks);
    void cleanupBookmarks();
    void cleanupStateBagHandlers();

    // Timers
    int32_t setTimeout(uint32_t ms, std::function<void()> cb);
    int32_t setInterval(uint32_t ms, std::function<void()> cb);
    void clearTimer(int32_t id);

    // Exports
    void addExport(const std::string& name, ExportHandler handler);
    json::Value callExport(const std::string& resource, const std::string& name, std::initializer_list<json::Value> args = {});

    // Emit
    void trace(const char* fmt, ...);
    void emit(const std::string& event, std::initializer_list<json::Value> args = {});
    void emitNet(const std::string& event, int target, std::initializer_list<json::Value> args = {});
    void cancelEvent();
    bool wasEventCanceled();

    // Statebags
    void setStateBagValue(const std::string& bagName, const std::string& key, const json::Value& value, bool replicated = true);
    void setPlayerState(int serverId, const std::string& key, const json::Value& value, bool replicated = true);
    void setEntityState(int netId, const std::string& key, const json::Value& value, bool replicated = true);
    void setGlobalState(const std::string& key, const json::Value& value, bool replicated = true);
    json::Value getStateBagValue(const std::string& bagName, const std::string& key);
    json::Value getPlayerState(int serverId, const std::string& key);
    json::Value getEntityState(int netId, const std::string& key);
    json::Value getGlobalState(const std::string& key);
    bool stateBagHasKey(const std::string& bagName, const std::string& key);
    std::vector<std::string> getStateBagKeys(const std::string& bagName);
    int32_t addStateBagChangeHandler(const std::string& keyFilter, const std::string& bagFilter, StateBagChangeHandler handler);
    void removeStateBagChangeHandler(int32_t cookie);

    // Metadata
    std::string getResourceMetadata(const std::string& key, int index = 0);
    int getNumResourceMetadata(const std::string& key);

    // Manifest version
    bool isManifestVersionBetween(const guid_t& lower, const guid_t& upper);
    bool isManifestVersionV2Between(const std::string& lower, const std::string& upper);

    // Resource introspection
    std::string getCurrentResourceName();
    std::string getInvokingResource();
    std::string getResourceState(const std::string& resource);
    int getNumResources();
    std::string getResourceByIndex(int index);

    bool hasPendingWork() const { return !m_tickHandlers.empty() || !m_timers.empty(); }
    void dispatchTick();
    void dispatchEvent(const std::string& name, const json::Value& args, const std::string& source);
    void dispatchCommand(const std::string& command, const std::string& source, const std::vector<std::string>& args);
    void dispatchStop();

    IScriptHost* getHost() { return m_host; }
    IScriptRuntime* getRuntime() { return m_runtime; }
    IScriptRuntimeHandler* getRuntimeHandler() { return m_handler.GetRef(); }
    IScriptHostWithResourceData* getMetadataHost() { return m_metadataHost.GetRef(); }
    const std::string& resourceName() const { return m_name; }

    void traceNativeError()
    {
        char* err = nullptr;
        if (FX_SUCCEEDED(m_host->GetLastErrorText(&err)) && err && err[0])
            trace("Native error: %s\n", err);
    }

    template<typename... Args>
    void invokeNative(uint64_t hash, Args... args)
    {
        static_assert(sizeof...(args) <= 32, "Native call exceeds 32-argument limit");
        fxNativeContext ctx{};
        ctx.nativeIdentifier = hash;
        size_t idx = 0;
        ((ctx.arguments[idx++] = static_cast<uintptr_t>(args)), ...);
        ctx.numArguments = static_cast<int>(idx);
        if (FX_FAILED(m_host->InvokeNative(ctx)))
            traceNativeError();
    }

    template<typename... Args>
    fxNativeContext invokeNativeResult(uint64_t hash, Args... args)
    {
        static_assert(sizeof...(args) <= 32, "Native call exceeds 32-argument limit");
        fxNativeContext ctx{};
        ctx.nativeIdentifier = hash;
        ctx.numResults = 3;
        size_t idx = 0;
        ((ctx.arguments[idx++] = static_cast<uintptr_t>(args)), ...);
        ctx.numArguments = static_cast<int>(idx);
        if (FX_FAILED(m_host->InvokeNative(ctx)))
            traceNativeError();
        return ctx;
    }

private:
    IScriptHost* m_host = nullptr;
    IScriptRuntime* m_runtime = nullptr;
    fx::OMPtr<IScriptRuntimeHandler> m_handler;
    AddRefFn m_addRef;
    RemoveRefFn m_removeRef;
    ScheduleBookmarkFn m_scheduleBookmark;
    fx::OMPtr<IScriptHostWithResourceData> m_metadataHost;
    fx::OMPtr<IScriptHostWithManifest> m_manifestHost;
    std::string m_name;
    std::unordered_map<std::string, std::vector<EventHandler>> m_eventHandlers;
    std::unordered_map<std::string, std::vector<CommandHandler>> m_commandHandlers;
    std::vector<TickHandler> m_tickHandlers;
    std::unordered_map<int32_t, TimerEntry> m_timers;
    int32_t m_nextTimerId = 1;
    std::unordered_set<std::string> m_netSafeEvents;
    std::vector<StopHandler> m_stopHandlers;
    std::unordered_map<uint64_t, std::pair<BookmarkHandle, std::shared_ptr<void>>> m_bookmarks;
    uint64_t m_nextBookmarkId = 1;
    std::unordered_map<int32_t, int32_t> m_stateBagHandlerRefs; // cookie -> refIdx
};

namespace detail { inline ResourceContext* g_ctx = nullptr; }
inline ResourceContext* GetContext() { return detail::g_ctx; }

}

#include "Impl/Events.inl"
#include "Impl/Lifecycle.inl"
#include "Impl/Timers.inl"
#include "Impl/Exports.inl"
#include "Impl/Emit.inl"
#include "Impl/Statebags.inl"
#include "Impl/Metadata.inl"

#define _FXCPP_ENTRY \
    static void _fxcpp_resource_body(fx::ResourceContext&); \
    extern "C" FXCPP_RESOURCE_EXPORT \
    void fxcpp_init(fx::ResourceContext* _ctx) \
    { \
        fx::detail::g_ctx = _ctx; \
        _fxcpp_resource_body(*_ctx); \
    } \
    static void _fxcpp_resource_body([[maybe_unused]] fx::ResourceContext& ctx)

#define Server _FXCPP_ENTRY
