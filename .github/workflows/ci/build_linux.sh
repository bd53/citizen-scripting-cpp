#!/bin/bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
MUSL_TARGET="x86_64-unknown-linux-musl"

cd "$PROJECT_DIR"

echo "Building wasmtime..."
cargo build --release -p wasmtime-c-api --target "$MUSL_TARGET" --manifest-path vendor/wasmtime/Cargo.toml

echo "Building runtime..."
python3 tools/code-gen/build.py
premake5 gmake2
make -C build config=release \
        CC="zig cc -target x86_64-linux-musl" \
        CXX="zig c++ -target x86_64-linux-musl" \
        -j"$(nproc)"

echo "Compiling resource (wasm)..."
tools/build tools/example/server.cpp

rm -f tools/example/server.wasm

echo "All checks passed."
