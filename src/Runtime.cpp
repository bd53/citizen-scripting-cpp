#include "Runtime.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>

static std::atomic<uint32_t> s_tempCounter{0};

static bool CopyFileTo(const std::string& src, const std::string& dst)
{
    FILE* in = fopen(src.c_str(), "rb");
    if (!in) return false;
    int fd = open(dst.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0700);
    if (fd < 0) { fclose(in); return false; }
    FILE* out = fdopen(fd, "wb");
    if (!out) { close(fd); fclose(in); return false; }
    char buf[8192];
    size_t n;
    bool ok = true;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n) { ok = false; break; }
    }
    fclose(in);
    fclose(out);
    if (!ok) std::remove(dst.c_str());
    return ok;
}

static std::string GetResourcePath(IScriptHost* host)
{
    fx::OMPtr<IScriptHostWithResourceData> md;
    fx::OMPtr<IScriptHost> h(host);
    if (FX_FAILED(h.As(&md)) || !md.GetRef())
        return {};
    char* name = nullptr;
    if (FX_FAILED(md->GetResourceName(&name)) || !name)
        return {};
    fxNativeContext ctx{};
    ctx.nativeIdentifier = HashString("GET_RESOURCE_PATH");
    ctx.arguments[0] = reinterpret_cast<uintptr_t>(name);
    ctx.numArguments = 1;
    ctx.numResults = 1;
    host->InvokeNative(ctx);
    const char* path = reinterpret_cast<const char*>(ctx.arguments[0]);
    return path ? std::string(path) : std::string{};
}

Runtime::Runtime() : m_instanceId(static_cast<int32_t>(reinterpret_cast<intptr_t>(this) & 0x7FFFFFFF))
{}

Runtime::~Runtime()
{
    Destroy();
}

result_t OM_DECL Runtime::Create(IScriptHost* host)
{
    m_host = host;
    fx::OMPtr<IScriptHost> h(host);
    fx::OMPtr<IScriptHostWithResourceData> md;
    if (FX_SUCCEEDED(h.As(&md)) && md.GetRef())
    {
        char* name = nullptr;
        if (FX_SUCCEEDED(md->GetResourceName(&name)) && name)
            m_resourceName = name;
    }
    return FX_S_OK;
}

result_t OM_DECL Runtime::Destroy()
{
    if (m_bookmarkHost.GetRef())
    {
        m_bookmarkHost->RemoveBookmarks(static_cast<IScriptTickRuntimeWithBookmarks*>(this));
        m_bookmarkHost = {};
    }
    if (m_ctx)
    {
        {
            fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
            m_ctx->dispatchStop();
            m_ctx->cleanupStateBagHandlers();
        }
        m_ctx->cleanupBookmarks();
        delete m_ctx;
        m_ctx = nullptr;
    }
    m_refs.clear();
    if (m_libHandle) { dlclose(m_libHandle); m_libHandle = nullptr; }
    if (!m_tempLibPath.empty())
    {
        std::remove(m_tempLibPath.c_str());
        m_tempLibPath.clear();
    }
    m_host = nullptr;
    return FX_S_OK;
}

void OM_DECL Runtime::SetParentObject(void* obj)
{
    m_parentObject = obj;
}

result_t OM_DECL Runtime::Tick()
{
    if (!m_ctx || !m_ctx->hasPendingWork()) return FX_S_OK;
    fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
    BoundaryGuard boundary(m_host, m_nextBoundaryId++);
    if (m_nextBoundaryId <= 0) m_nextBoundaryId = 1;
    try
    {
        m_ctx->dispatchTick();
    }
    catch (const std::exception& e)
    {
        m_ctx->trace("Unhandled exception in tick handler: %s\n", e.what());
    }
    catch (...)
    {
        m_ctx->trace("Unhandled non-standard exception in tick handler\n");
    }
    return FX_S_OK;
}

result_t OM_DECL Runtime::TickBookmarks(uint64_t* bookmarks, int32_t numBookmarks)
{
    if (!m_ctx || numBookmarks <= 0) return FX_S_OK;
    fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
    BoundaryGuard boundary(m_host, m_nextBoundaryId++);
    if (m_nextBoundaryId <= 0) m_nextBoundaryId = 1;
    m_ctx->resumeBookmarks(bookmarks, numBookmarks);
    return FX_S_OK;
}

result_t OM_DECL Runtime::TriggerEvent(char* eventName, char* argsSerialized, uint32_t serializedSize, char* sourceId)
{
    if (!m_ctx || !eventName) return FX_S_OK;
    fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
    BoundaryGuard boundary(m_host, m_nextBoundaryId++);
    if (m_nextBoundaryId <= 0) m_nextBoundaryId = 1;
    try
    {
        fx::json::Value args = fx::msgpack::decode(argsSerialized, serializedSize);
        fx::json::ensureArray(args);
        std::string src = sourceId ? sourceId : "-1";
        m_ctx->dispatchEvent(eventName, args, src);
    }
    catch (const std::exception& e)
    {
        m_ctx->trace("Unhandled exception in event '%s': %s\n", eventName, e.what());
    }
    catch (...)
    {
        m_ctx->trace("Unhandled non-standard exception in event '%s'\n", eventName);
    }
    return FX_S_OK;
}

int32_t Runtime::AddFuncRef(fx::RefCallback cb)
{
    int32_t idx = m_nextRefIdx++;
    if (m_nextRefIdx <= 0) m_nextRefIdx = 1;
    m_refs[idx] = std::move(cb);
    return idx;
}

result_t OM_DECL Runtime::CallRef(int32_t refIdx, char* argsSerialized, uint32_t argsSize, IScriptBuffer** retval)
{
    auto it = m_refs.find(refIdx);
    if (it == m_refs.end()) return FX_E_INVALIDARG;
    fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
    BoundaryGuard boundary(m_host, m_nextBoundaryId++);
    if (m_nextBoundaryId <= 0) m_nextBoundaryId = 1;
    std::vector<char> result;
    try
    {
        result = it->second(argsSerialized, argsSize);
    }
    catch (const std::exception& e)
    {
        if (m_ctx)
            m_ctx->trace("Unhandled exception in ref %d: %s\n", refIdx, e.what());
        result = { static_cast<char>(0x90) }; // msgpack empty array
    }
    catch (...)
    {
        if (m_ctx)
            m_ctx->trace("Unhandled non-standard exception in ref %d\n", refIdx);
        result = { static_cast<char>(0x90) };
    }
    auto buf = fx::MakeNew<ScriptBuffer>(std::move(result));
    return buf->QueryInterface(IScriptBuffer::GetIID(), reinterpret_cast<void**>(retval));
}

result_t OM_DECL Runtime::DuplicateRef(int32_t refIdx, int32_t* newRefIdx)
{
    auto it = m_refs.find(refIdx);
    if (it == m_refs.end()) return FX_E_INVALIDARG;
    *newRefIdx = m_nextRefIdx++;
    if (m_nextRefIdx <= 0) m_nextRefIdx = 1;
    m_refs[*newRefIdx] = it->second;
    return FX_S_OK;
}

result_t OM_DECL Runtime::RemoveRef(int32_t refIdx)
{
    m_refs.erase(refIdx);
    return FX_S_OK;
}

result_t OM_DECL Runtime::WalkStack(char* boundaryStart, uint32_t boundaryStartLength, char* boundaryEnd, uint32_t boundaryEndLength, IScriptStackWalkVisitor* visitor)
{
    return FX_S_OK;
}

result_t OM_DECL Runtime::RequestMemoryUsage()
{
    return FX_S_OK;
}

result_t OM_DECL Runtime::GetMemoryUsage(int64_t* memUsage)
{
    if (!memUsage) return FX_E_INVALIDARG;
    *memUsage = 0;
    return FX_S_OK;
}

result_t OM_DECL Runtime::EmitWarning(char* channel, char* message)
{
    if (m_ctx && message)
    {
        const char* ch = channel ? channel : "script";
        m_ctx->trace("[warning:%s] %s\n", ch, message);
    }
    return FX_S_OK;
}

// @todo
void OM_DECL Runtime::SetupFxProfiler(void*, int32_t)
{
}

// @todo
void OM_DECL Runtime::ShutdownFxProfiler()
{
}

int32_t OM_DECL Runtime::HandlesFile(char* scriptFile, IScriptHostWithResourceData* /*metadata*/)
{
    if (!scriptFile) return 0;
    std::string_view file(scriptFile);
    return file.ends_with(".so") ? 1 : 0;
}

result_t OM_DECL Runtime::LoadFile(char* scriptFile)
{
    if (!m_host || !scriptFile) return FX_E_INVALIDARG;
    std::string root = GetResourcePath(m_host);
    if (!root.empty() && root.back() == '/')
        root.pop_back();
    if (root.empty())
    {
        fprintf(stderr, "[citizen-scripting-cpp] Runtime: could not get resource path for '%s'\n", m_resourceName.c_str());
        return FX_E_INVALIDARG;
    }
    std::string_view scriptFileView(scriptFile);
    if (scriptFileView.find("/..") != std::string_view::npos || scriptFileView.find("../") != std::string_view::npos || scriptFileView == "..")
    {
        fprintf(stderr, "[citizen-scripting-cpp] Rejected script path with '..': '%s'\n", scriptFile);
        return FX_E_INVALIDARG;
    }
    {
        fx::OMPtr<fxIStream> stream;
        if (FX_FAILED(m_host->OpenHostFile(scriptFile, stream.GetAddressOf())) || !stream.GetRef())
        {
            fprintf(stderr, "[citizen-scripting-cpp] Host denied access to '%s' in resource '%s'\n", scriptFile, m_resourceName.c_str());
            return FX_E_INVALIDARG;
        }
    }

    std::string fullPath = root + "/" + scriptFile;
    uint32_t seq = s_tempCounter.fetch_add(1, std::memory_order_relaxed);
    std::string loadPath;
    {
        std::string tmpDir = "/tmp";
        std::string baseName = m_resourceName + "_" + std::string(scriptFile);
        for (auto& c : baseName) { if (c == '/' || c == '\\') c = '_'; }
        loadPath = tmpDir + "/" + baseName + ".rt." + std::to_string(getpid()) + "." + std::to_string(seq);
    }
    if (!CopyFileTo(fullPath, loadPath))
    {
        fprintf(stderr, "[citizen-scripting-cpp] Failed to copy '%s' to temp path '%s', loading original\n", fullPath.c_str(), loadPath.c_str());
        loadPath = fullPath;
    }

    m_libHandle = dlopen(loadPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!m_libHandle)
    {
        fprintf(stderr, "[citizen-scripting-cpp] dlopen failed for '%s': %s\n", loadPath.c_str(), dlerror());
        if (loadPath != fullPath) std::remove(loadPath.c_str());
        return FX_E_INVALIDARG;
    }
    auto* initFn = reinterpret_cast<void(*)(fx::ResourceContext*)>(dlsym(m_libHandle, "fxcpp_init"));
    m_tempLibPath = (loadPath != fullPath) ? loadPath : std::string{};
    if (!initFn)
    {
        fprintf(stderr, "[citizen-scripting-cpp] '%s' has no fxcpp_init export\n", fullPath.c_str());
        return FX_E_INVALIDARG;
    }
    fx::OMPtr<IScriptRuntimeHandler> runtimeHandler;
    fx::MakeInterface(&runtimeHandler, CLSID_ScriptRuntimeHandler);
    {
        fx::OMPtr<IScriptHost> sh(m_host);
        fx::OMPtr<IScriptHostWithBookmarks> bh;
        if (FX_SUCCEEDED(sh.As(&bh)) && bh.GetRef())
        {
            m_bookmarkHost = bh;
            m_bookmarkHost->CreateBookmarks(static_cast<IScriptTickRuntimeWithBookmarks*>(this));
        }
    }
    fx::ScheduleBookmarkFn schedBookmark;
    if (m_bookmarkHost.GetRef())
    {
        schedBookmark = [this](uint64_t bm, int64_t deadline) {
            if (m_bookmarkHost.GetRef())
                m_bookmarkHost->ScheduleBookmark(static_cast<IScriptTickRuntimeWithBookmarks*>(this), bm, deadline);
        };
    }
    m_ctx = new fx::ResourceContext(m_host, this, m_resourceName, runtimeHandler.GetRef(), [this](fx::RefCallback cb) -> int32_t { return AddFuncRef(std::move(cb)); }, [this](int32_t idx) { m_refs.erase(idx); }, std::move(schedBookmark));
    fprintf(stderr, "[citizen-scripting-cpp] Loaded C++ resource '%s'\n", m_resourceName.c_str());
    fx::PushEnvironment env(static_cast<IScriptRuntime*>(this));
    try
    {
        initFn(m_ctx);
    }
    catch (const std::exception& e)
    {
        fprintf(stderr, "[citizen-scripting-cpp] Exception during init of '%s': %s\n", m_resourceName.c_str(), e.what());
        m_ctx->trace("Exception during resource init: %s\n", e.what());
        if (m_bookmarkHost.GetRef())
        {
            m_bookmarkHost->RemoveBookmarks(static_cast<IScriptTickRuntimeWithBookmarks*>(this));
            m_bookmarkHost = {};
        }
        m_ctx->cleanupBookmarks();
        delete m_ctx; m_ctx = nullptr;
        if (m_libHandle) { dlclose(m_libHandle); m_libHandle = nullptr; }
        if (!m_tempLibPath.empty()) { std::remove(m_tempLibPath.c_str()); m_tempLibPath.clear(); }
        return FX_E_INVALIDARG;
    }
    catch (...)
    {
        fprintf(stderr, "[citizen-scripting-cpp] Non-standard exception during init of '%s'\n", m_resourceName.c_str());
        if (m_bookmarkHost.GetRef())
        {
            m_bookmarkHost->RemoveBookmarks(static_cast<IScriptTickRuntimeWithBookmarks*>(this));
            m_bookmarkHost = {};
        }
        m_ctx->cleanupBookmarks();
        delete m_ctx; m_ctx = nullptr;
        if (m_libHandle) { dlclose(m_libHandle); m_libHandle = nullptr; }
        if (!m_tempLibPath.empty()) { std::remove(m_tempLibPath.c_str()); m_tempLibPath.clear(); }
        return FX_E_INVALIDARG;
    }
    return FX_S_OK;
}
