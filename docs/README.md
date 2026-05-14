# Get Started

[citizen-scripting-cpp](https://github.com/bd53/citizen-scripting-cpp) lets you write resources in C++, compiled as native shared libraries (`.so`) or WebAssembly modules (`.wasm`).

## Quick links

- [Notice](Notice.md) - how it works, notes, limitations, etc 
- [Setup](Setup.md) - build the runtime and your first resource
- [Examples](Examples/Overview.md) - events, natives, exports, timers, coroutines, and more

## Differences

### Engine and execution

| | Lua | Node | Mono-v2 | .so | .wasm |
|---|---|---|---|---|---|
| Engine | Lua 5.4 | V8 + Node.js | Mono JIT | Native (dlopen) | wasmtime |
| Language std | Lua 5.4 | ES2020+ | .NET | C++23 | C++23 |
| GC | Incremental | V8 GC | Mono GC | None (manual) | None (manual) |
| Sandboxing | Restricted stdlib | Filesystem permissions | AppDomain isolation | None | WASM sandbox |
| Custom allocator | rpmalloc | V8 heap limits | Mono allocator | System malloc | WASM linear memory |

### Interfaces implemented

All runtimes implement `IScriptRuntime`, `IScriptFileHandlingRuntime`, `IScriptEventRuntime`, and `IScriptRefRuntime`.

| Interface | Lua | Node | Mono-v2 | C++ |
|---|---|---|---|---|
| `IScriptTickRuntimeWithBookmarks` | Yes | No | No | Yes |
| `IScriptTickRuntime` | No | Yes | Yes (tick-less) | Yes |
| `IScriptStackWalkingRuntime` | Yes | Yes | No | No-op |
| `IScriptMemInfoRuntime` | Yes | No | Yes | Yes |
| `IScriptProfiler` | Yes | No (uses V8 CpuProfiler) | Yes | No-op |
| `IScriptDebugRuntime` | Yes | No | Yes | No |
| `IScriptWarningRuntime` | Yes | Yes | No | Yes |

### Scheduling

| | Lua | Node | Mono-v2 | C++ |
|---|---|---|---|---|
| Tick model | Bookmark scheduler | UV loop timer | Tick-less (scheduled time) | Bookmark scheduler |
| Coroutines | `CreateThread` / `Wait` | `Promises` / `async-await` | `async` / `await` (Task) | `co_await` / `fx::Wait{}` |

Mono-v2's tick-less optimization skips rntime entry/exit entirely when there's no scheduled work, `GetCurrentSchedulerTime() < m_sharedData.m_scheduledTime` short-circuits the tick.

Lua and C++ use the bookmark schedlr (`IScriptTickRuntimeWithBookmarks`), which lets the host call `TickBookmarks` with only the bookmarks that are ready, avoiding unnecessary tick overhead.

### Stdlib restrictions

| | Lua | Node | Mono-v2 | .so | .wasm |
|---|---|---|---|---|---|
| File I/O | `io`/`os` (server only) | Filesystem permission callbacks | Full .NET IO | Full POSIX | WASI (host-controlled) |
| Spawning processes | No | `child_process` (permission-gated) | `System.Diagnostics` | Full access | `fx::spawnProcess` (permission-gated) |
| Workers/threads | No | `worker_threads` (permission-gated) | `System.Threading` | Full access | `fx::createWorker` (permission-gated) |
