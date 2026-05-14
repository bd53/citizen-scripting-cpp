#!/bin/bash
set -e

SERVER_DIR="${FX_SERVER_DIR:?Set FX_SERVER_DIR to your cfx-server directory}"
RESOURCE_DIR="${FX_RESOURCE_DIR:?Set FX_RESOURCE_DIR to your resource directory}"
PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
WASMTIME_LIB="$PROJECT_DIR/vendor/wasmtime/target/x86_64-unknown-linux-musl/release/libwasmtime.a"

BUILD_TYPE="so"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --type=*) BUILD_TYPE="${1#--type=}"; shift ;;
        --type) BUILD_TYPE="$2"; shift 2 ;;
        *) shift ;;
    esac
done

cd "$PROJECT_DIR"

rm -rf "$BUILD_DIR"

if [[ ! -f "$WASMTIME_LIB" ]]; then
    git -C "$PROJECT_DIR" submodule update --init --depth=1 vendor/wasmtime
    rustup target add x86_64-unknown-linux-musl
    cargo build --release -p wasmtime-c-api \
        --target x86_64-unknown-linux-musl \
        --manifest-path "$PROJECT_DIR/vendor/wasmtime/Cargo.toml"
fi

python3 tools/code-gen/build.py
premake5 gmake2 --wasm

echo "Building..."
make -C "$BUILD_DIR" config=release \
    CC="zig cc -target x86_64-linux-musl" \
    CXX="zig c++ -target x86_64-linux-musl" \
    -j"$(nproc)"

if [[ "$BUILD_TYPE" == "wasm" ]]; then
    echo "Compiling example resource to WASM..."
    clang++ --target=wasm32-wasip1 -O2 -std=c++23 \
        --sysroot=/usr/share/wasi-sysroot \
        -fno-exceptions \
        -I"$PROJECT_DIR" \
        -Wl,--export-memory \
        "$PROJECT_DIR/tools/example/server.cpp" \
        -o "$BUILD_DIR/bin/Release/server.wasm"
fi

cp "$BUILD_DIR/bin/Release/libcitizen-scripting-cpp.so" "$SERVER_DIR/"
mkdir -p "$RESOURCE_DIR"
rm -f "$RESOURCE_DIR/server.so" "$RESOURCE_DIR/server.wasm" "$RESOURCE_DIR/fxmanifest.lua"

if [[ "$BUILD_TYPE" == "wasm" ]]; then
    cp "$BUILD_DIR/bin/Release/server.wasm" "$RESOURCE_DIR/server.wasm"
    sed 's/server\.so/server.wasm/' "$PROJECT_DIR/tools/example/fxmanifest.lua" \
        > "$RESOURCE_DIR/fxmanifest.lua"
else
    cp "$BUILD_DIR/bin/Release/server.so" "$RESOURCE_DIR/server.so"
    cp -a "$PROJECT_DIR/tools/example/." "$RESOURCE_DIR/"
fi

echo "Done. Runtime -> $SERVER_DIR, resource -> $RESOURCE_DIR"
