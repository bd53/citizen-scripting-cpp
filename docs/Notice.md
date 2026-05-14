# Notice

## Why C++?

- Native code with zero interop overhead, no GC pauses
- Optional WebAssembly mode isolates resources
- Can't think of any other reason to be honest

## How it works

C++ resources are a shared library (`.so`) or WebAssembly module (`.wasm`) that exports an entry point.

The runtime plugin (`libcitizen-scripting-cpp.so`) is loaded by FXServer and handles the lifecycle:

1. Server reads `fxmanifest.lua` -> sees `server_script 'server.so'` (or `.wasm`)
2. Runtime loads your library and calls `fxcpp_init`
3. `Server { }` block runs and it registers events, exports, timers, etc.
4. Runtime dispatches ticks, events, and ref calls to your handlers
5. On resource stop, `onStop` handlers run and everything is cleaned up

## Important notes

- This runtime is **server-side** only, client-side is not yet supported.
- Linux is the only supported platform. If someone wants to make a PR for Windows support, all contributions are accepted.
- Native database (`Native/DB.h`) is auto-generated and will need to be regenerated as natives are added or changed upstream.

## Security

- `.so` resources run as native code and have full access to the host system, only load `.so` resources you trust.
- Loading a `.so` resource is blocked by default, requires `sv_allowNativeCode true` in .cfg.
- `.wasm` resources are sandboxed via wasmtime, they can only interact with the server through the defined host imports.
- `.wasm` child process spawning requires `set sv_wasmChildProcess "resource-name"` (or `"*"` for all) in your .cfg.
- `.wasm` worker threads require `set sv_wasmWorkerThreads "resource-name"` (or `"*"` for all) in your .cfg.
- Net events are filtered so only events registered with `fx::onNet` accept calls from clients, matching other ScRT's.

## Limitations

- As mentioned previosly, client-side is **not** yet supported.
- WASM resources cannot use C++ exceptions (`-fno-exceptions` is required).
- `IScriptStackWalkingRuntime` is no-op. C++ resources won't appear in cross-runtime stack traces from `FORMAT_STACK_TRACE`.
- `IScriptProfiler` is not yet implemented. `profiler record` won't produce scope events from C++ resources.
- Probably some other stuff somewhere

## Compatibility

- The C++ API (events, exports, natives, timers, coroutines, statebags, etc.) has full parity with Lua, JS, and Mono-v2.
- The same source file can target both `.so` and `.wasm`, only the `#include` differs (`SDK.h` vs `WASM.h`), all API calls are identical.
