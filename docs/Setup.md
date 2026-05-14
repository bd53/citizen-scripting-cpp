# Setup

This guide covers building the runtime plugin and compiling your first C++ resource.

## Prerequisites

- Linux x86_64
- g++ / C++23
- [premake5](https://premake.github.io/)
- [FXServer](https://runtime.fivem.net/artifacts/fivem/build_proot_linux/master/)

For WASM support, you'll additionally need:
- [Rust toolchain](https://rustup.rs/) (for building wasmtime)
- [Zig](https://ziglang.org/) (for musl cross-compilation)
- clang++ with [wasi-sysroot](https://github.com/WebAssembly/wasi-sdk)

## Build the runtime plugin

### .so (native resources only)

```bash
git clone https://github.com/bd53/citizen-scripting-cpp.git citizen-scripting-cpp
cd citizen-scripting-cpp
premake5 gmake2
cd build && make -f citizen-scripting-cpp.make config=release
```

### .so + .wasm (native and WebAssembly resources)

```bash
git clone --recursive https://github.com/bd53/citizen-scripting-cpp.git citizen-scripting-cpp
cd citizen-scripting-cpp

# Build wasmtime (first time only)
cargo build --release -p wasmtime-c-api \
  --target x86_64-unknown-linux-musl \
  --manifest-path vendor/wasmtime/Cargo.toml

premake5 gmake2 --wasm
cd build && make -f citizen-scripting-cpp.make config=release \
  CC="zig cc -target x86_64-linux-musl" \
  CXX="zig c++ -target x86_64-linux-musl" \
  -j$(nproc)
```

## Install the runtime

Copy `build/bin/Release/libcitizen-scripting-cpp.so` into your FXServer directory (next to the server binary).

## Create your resource

### Directory structure

```
resources/
  - my-resource/
    - fxmanifest.lua
    - server.cpp
```

### fxmanifest.lua

```lua
fx_version 'cerulean'
game 'gta5'

server_script 'server.so'
-- or for WASM: server_script 'server.wasm'
```

### server.cpp

```cpp
#ifdef __wasm__
#include <src/WASM.h>
#else
#include <src/SDK.h>
#endif

Server
{
    fx::trace("Hello from C++!\n");

    fx::onStop([] {
        fx::trace("Goodbye from C++!\n");
    });
};
```

## Compile your resource

Use the `build` tool found in the `tools/` directory:

```bash
# Native (.so)
/path/to/citizen-scripting-cpp/tools/build server.cpp -o server.so

# WebAssembly (.wasm)
/path/to/citizen-scripting-cpp/tools/build server.cpp -o server.wasm
```

The tool auto-detects the target from the output extension and sets up all compiler flags, includes, and sysroot paths.

For `.wasm`, it uses `clang++` with `--target=wasm32-wasip1` and the WASI sysroot (auto-detected, or set `WASI_SYSROOT`). 

For `.so`, it uses `g++` with `-shared -fPIC`.

Extra flags are passed through to the compiler (e.g. `-O3`, `-Wall`).

## Automated build and deploy

The `tools/deploy` utilty handle full pipeline: code generation, premake, compilation, and deployment of the runtime.

```bash
# Set your server and resource paths
export FX_SERVER_DIR=/path/to/cfx-server
export FX_RESOURCE_DIR=/path/to/resources/my-resource

# Build and deploy (.so resource)
tools/deploy

# Build and deploy (.wasm resource)
tools/deploy --type wasm
```

This will:
1. Initialize the wasmtime submodule (if needed)
2. Run native database code generation (`tools/code-gen/build.py`)
3. Generate Makefiles via premake5 and compile the runtime with musl libc
4. Compile the example resource
5. Copy the runtime `.so` to your `cfx-server/` dir and the example resource to your `resources/` dir

### Code generation

The native database (`src/Native/DB.h`) is auto-generated from upstream native definitions.

If you need to regenerate it (e.g. after upstream native changes):

```bash
python3 tools/code-gen/build.py
```

This runs automatically as part of `tools/deploy` but can also run standalone.

## Deploy and run

1. Copy `server.so` (or `server.wasm`) into your `resources/` directory
2. For `.so` resources, add `sv_allowNativeCode true` to your .cfg (not required for `.wasm`)
3. Add `ensure my-resource` to where resources are being loaded

You should see `Hello from C++!` in the console.

## Next steps

Check out the [Examples](Examples/Overview.md) for events, natives, exports, timers, coroutines, and more.
