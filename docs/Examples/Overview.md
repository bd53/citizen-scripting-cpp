# Examples

All examples work for both `.so` and `.wasm` resources, just use the appropriate `#include`:

```cpp
#ifdef __wasm__
#include <src/WASM.h>
#else
#include <src/SDK.h>
#endif
```

1. [Events](#events)
2. [Net events](#net-events)
3. [Data types](#data-types)
4. [Natives](#natives)
5. [Exports](#exports)
6. [Timers](#timers)
7. [Tick handler](#tick-handler)
8. [Coroutines](#coroutines)
9. [Statebags](#statebags)
10. [Resource metadata](#resource-metadata)
11. [KVP (Key-Value Pairs)](#kvp-key-value-pairs)
12. [Child process](#child-process)
13. [Worker threads](#worker-threads)

## Events

```cpp
Server
{
    // Typed parameters, args are extracted automatically
    fx::on("playerConnecting", [](const std::string& source, std::string name) {
        fx::trace("Player %s connecting from source %s\n", name.c_str(), source.c_str());
    });

    // EventArgs style, manual extraction (useful when arg count varies)
    fx::on("flexibleEvent", [](const std::string& source, fx::EventArgs args) {
        if (args.size() > 0)
            fx::trace("Got: %s\n", args.get<std::string>(0).c_str());
    });

    // Emit an event
    fx::emit("myEvent", "hello", 42);

    // Cancel an event (inside a handler)
    fx::on("someEvent", [](const std::string&) {
        fx::cancelEvent();
    });
};
```

## Net events

Net events are filtered by default, only events registered with `onNet` accept network-originating calls.

```cpp
Server
{
    // Typed parameters
    fx::onNet("chatMessage", [](const std::string& source, int author, std::string name, std::string message) {
        fx::trace("[chat] %s: %s\n", name.c_str(), message.c_str());
    });

    // EventArgs style also works
    fx::onNet("otherEvent", [](const std::string& source, fx::EventArgs args) {
        fx::trace("Got %zu args\n", args.size());
    });

    // Send an event to a specific client
    fx::emitNet("showNotification", 1, "Hello player 1!");

    // Send to all clients
    fx::emitNet("showNotification", -1, "Hello everyone!");
};
```

## Data types

All cross-resource communication (events, exports, function references) uses [MsgPack](https://msgpack.org/) as the wire format.

The runtime handles serialization and deserialization automatically.

### Sending (emit, emitNet, export return values)

| C++ type | MsgPack type |
|---|---|
| `int`, `int32_t`, `int64_t`, `uint32_t` | Integer |
| `float`, `double` | Float64 |
| `bool` | Boolean |
| `const char*`, `std::string` | String |
| `nullptr` / omitted | Nil |

### Receiving

Event handlers support two styles.

Typed parameters extract args automatically at compile time:

```cpp
fx::on("playerInfo", [](const std::string& source, std::string name, int age, double score, bool active) {
    // name, age, score, active are extracted automatically from the MsgPack payload
    fx::trace("%s is %d years old\n", name.c_str(), age);
});
```

EventArgs style gives you manual control (useful when arg count varies):

```cpp
fx::on("playerInfo", [](const std::string& source, fx::EventArgs args) {
    std::string name = args.get<std::string>(0);
    int age = args.get<int>(1);
    double score = args.get<double>(2);
    bool active = args.get<bool>(3);
    size_t count = args.size();
    bool missing = args.isNull(10); // true (out of bounds)
});
```

Available `get<T>` specializations:

| `get<T>()` | Returns |
|---|---|
| `get<std::string>(i)` | String value (empty if not a string) |
| `get<int>(i)` | Integer value (0 if not a number) |
| `get<double>(i)` | Double value (0.0 if not a number) |
| `get<float>(i)` | Float value (0.0f if not a number) |
| `get<bool>(i)` | Boolean value (false if not a bool) |

Named accessors are also available: `args.str(i)`, `args.integer(i)`, `args.number(i)`, `args.boolean(i)`, `args.isNull(i)`, `args.funcRef(i)`.

For native return values, `fx::Vector3` is also supported:

```cpp
fx::Vector3 pos = fx::natives::invoke<fx::Vector3>(hash, ...);
// pos.x, pos.y, pos.z
```

### Cross-runtime compatibility

Events and exports sent from C++ can be received by Lua, JavaScript, and C# resources (and vice versa).

| C++ | Lua | JavaScript | C# |
|---|---|---|---|
| `std::string` / `const char*` | `string` | `string` | `string` |
| `int` / `int64_t` / `uint32_t` | `number` | `number` | `int` / `long` |
| `double` / `float` | `number` | `number` | `double` / `float` |
| `bool` | `boolean` | `boolean` | `bool` |
| `nullptr` | `nil` | `null` / `undefined` | `null` |

Function references (callbacks passed between resources) are serialized as MsgPack ext type 10 and handled by the export system.

## Natives

Natives are invoked through a generated database.

All CFX and game natives are available.

```cpp
Server
{
    fx::on("playerJoining", [](const std::string& source, fx::EventArgs args) {
        std::string id = source.substr(source.find(':') + 1);

        // Get player info via natives
        std::string name = fx::natives::cfx::GetPlayerName(id.c_str());
        int ping = fx::natives::cfx::GetPlayerPing(id.c_str());
        std::string license = fx::natives::cfx::GetPlayerIdentifierByType(id.c_str(), "license2");

        fx::trace("%s joined (ping: %dms, license: %s)\n", name.c_str(), ping, license.c_str());
    });
};
```

## Exports

```cpp
Server
{
    // Register an export
    fx::addExport("getGreeting", [](fx::EventArgs args) -> fx::json::Value {
        std::string name = args.get<std::string>(0);
        return "Hello, " + name + "!";
    });

    // Call an export from another resource (or the same resource ;))
    fx::json::Value result = fx::callExport("other-resource", "someExport", {42, "arg"});
    fx::trace("Result: %s\n", result.asStr("none").c_str());
};
```

## Timers

```cpp
Server
{
    // Run once after 5 seconds
    fx::setTimeout(5000, [] {
        fx::trace("5 seconds passed!\n");
    });

    // Run every 10 seconds
    int32_t id = fx::setInterval(10000, [] {
        fx::trace("Another 10 seconds...\n");
    });

    // Cancel a timer
    fx::clearTimer(id);
};
```

## Tick handler

`onTick` registers a callback that runs every server frame.

This is the equivalent of `CreateThread` with `Wait()`.

```cpp
Server
{
    fx::onTick([] {
        // Runs every frame - use for polling, continuous checks, etc.
    });
};
```

## Coroutines

Coroutines let you write async code with `co_await`.

They use a bookmark scheduler (`.so`) or a built-in scheduler (`.wasm`).

```cpp
Server
{
    fx::createThread([]() -> fx::ScriptTask {
        fx::trace("Coroutine started\n");

        co_await fx::Wait{1000};
        fx::trace("1 second later\n");

        co_await fx::Wait{2000};
        fx::trace("2 more seconds later\n");

        // Loop with a delay
        for (int i = 0; i < 10; i++)
        {
            co_await fx::Wait{500};
            fx::trace("Tick %d\n", i);
        }
    });
};
```

## Statebags

```cpp
Server
{
    // Set global state
    fx::setGlobalState("serverName", "My Server", true);

    // Set player / entity state
    fx::setPlayerState(1, "score", 100);
    fx::setEntityState(42, "color", "red");

    // Get state
    fx::json::Value name = fx::getGlobalState("serverName");
    fx::trace("Server: %s\n", name.asStr().c_str());

    // Check if a key exists
    bool exists = fx::stateBagHasKey("global", "serverName");

    // Get all keys in a bag
    std::vector<std::string> keys = fx::getStateBagKeys("global");

    // Listen for changes (empty filters = match all)
    int32_t cookie = fx::addStateBagChangeHandler("", "", [](const std::string& bagName, const std::string& key,
        const fx::json::Value& value, int source, bool replicated) {
        fx::trace("%s:%s changed (source=%d)\n", bagName.c_str(), key.c_str(), source);
    });

    // Remove a change handler when no longer needed
    fx::removeStateBagChangeHandler(cookie);
};
```

Bag names follow the convention: `"global"` for global state, `"player:1"` for player server ID 1, `"entity:42"` for entity net ID 42.

The convenience functions (`setGlobalState`, `setPlayerState`, `setEntityState`) build these names for you.

## Resource metadata

```cpp
Server
{
    // Read fxmanifest values
    std::string name = fx::getResourceMetadata("name");
    std::string author = fx::getResourceMetadata("author");
    std::string version = fx::getResourceMetadata("version");

    fx::trace("Resource: %s v%s by %s\n", name.c_str(), version.c_str(), author.c_str());

    // Current resource name (as seen by the server)
    std::string resName = fx::getCurrentResourceName();

    // Get the name of the resource that invoked this one (via exports, etc.)
    std::string invoker = fx::getInvokingResource();

    // Enumerate all resources
    std::vector<std::string> resources = fx::getResources();
    for (const auto& r : resources)
        fx::trace("  - %s (%s)\n", r.c_str(), fx::getResourceState(r).c_str());
};
```

## KVP (Key-Value Pairs)

Persistent key-value storage.

```cpp
Server
{
    // Store values
    fx::setKvp("greeting", "hello world");
    fx::setKvpInt("counter", 42);
    fx::setKvpFloat("ratio", 3.14f);

    // Read values
    std::string greeting = fx::getKvpString("greeting");
    int counter = fx::getKvpInt("counter");
    float ratio = fx::getKvpFloat("ratio");

    // Find keys by prefix
    std::vector<std::string> keys = fx::findKvp("counter");

    // Delete
    fx::deleteKvp("greeting");

    // Flush pending writes to disk
    fx::flushKvp();
};
```

## Child process

Spawn a child process.

For `.wasm`, this is permission-gated, requires `set sv_wasmChildProcess "resource-name"` (or `"*"`) in .cfg. `.so` resources don't need this.

```cpp
Server
{
    fx::ProcessResult result = fx::spawnProcess("echo hello");

    if (result.status == -1)
        fx::trace("Permission denied - check sv_wasmChildProcess\n");
    else if (result.status == -2)
        fx::trace("Failed to spawn process\n");
    else
        fx::trace("Output: %s\n", result.output.c_str());
};
```

## Worker threads

Run a function in a separate thread.

`.wasm` spawns a fresh WASM instance, and `.so` uses `std::thread`.

`.wasm` workers are permission-gated, requires `set sv_wasmWorkerThreads "resource-name"` (or `"*"`) in .cfg. `.so` resources don't need this.

### Define a worker function

```cpp
// Must be outside the Server {} block
FXCPP_WORKER(my_worker)
{
    // input, input_len, result, result_max are provided by the macro
    std::string data(input, input_len);

    // Do heavy computation...
    std::string out = "processed: " + data;

    int32_t copy = static_cast<int32_t>(out.size());
    if (copy > result_max - 1) copy = result_max - 1;
    memcpy(result, out.data(), copy);
    result[copy] = '\0';
    return copy;
}
```

### Start and poll

```cpp
Server
{
    int32_t wid = fx::createWorker("my_worker", "some input data");

    if (wid == -1)
        fx::trace("Permission denied - check sv_wasmWorkerThreads\n");
    else
    {
        // Poll from a timer (workers run asynchronously)
        fx::setTimeout(100, [wid] {
            fx::WorkerResult wr = fx::pollWorker(wid);
            if (wr.status > 0)
                fx::trace("Worker result: %s\n", wr.output.c_str());
            else if (wr.status == 0)
                fx::trace("Worker still running...\n");
            else
                fx::trace("Worker error\n");
        });
    }
};
```
