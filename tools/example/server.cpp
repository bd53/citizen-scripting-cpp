#ifdef __wasm__
#include <src/WASM.h>
#else
#include <src/SDK.h>
#endif
#include <memory>
#include <unordered_map>

static std::string extractId(const std::string& source)
{
    auto pos = source.find(':');
    return pos != std::string::npos ? source.substr(pos + 1) : source;
}

Server
{
    auto pending = std::make_shared<std::unordered_map<std::string, std::string>>();
    auto players = std::make_shared<std::unordered_map<std::string, std::string>>();

    auto meta = [](const std::string& key) { return fx::getResourceMetadata(key); };

    if (!meta("fx_version").empty()) fx::trace("Manifest: %s\n", meta("fx_version").c_str());
    if (!meta("game").empty()) fx::trace("Game: %s\n", meta("game").c_str());
    fx::trace("Name: %s\n", meta("name").c_str());
    if (!meta("author").empty()) fx::trace("Author: %s\n", meta("author").c_str());
    if (!meta("description").empty()) fx::trace("Description: %s\n", meta("description").c_str());
    if (!meta("version").empty()) fx::trace("Version: %s\n", meta("version").c_str());

    int server = fx::getNumResourceMetadata("server_script");
    if (server > 0)
    {
        std::string scripts = "Server Scripts: ";
        for (int i = 0; i < server; i++)
        {
            if (i > 0) scripts += ", ";
            scripts += fx::getResourceMetadata("server_script", i);
        }
        fx::trace("%s\n", scripts.c_str());
    }

    fx::onStop([players]() {
        fx::trace("Resource stopping, %zu players tracked.\n", players->size());
    });

    fx::on("playerConnecting", [pending](const std::string& source, std::string name) {
        (*pending)[extractId(source)] = name;
    });

    fx::on("playerJoining", [pending, players](const std::string& source, std::string oldId) {
        auto it = pending->find(oldId);
        std::string name = (it != pending->end()) ? it->second : "Unknown";
        if (it != pending->end()) pending->erase(it);
        std::string id = extractId(source);
        (*players)[id] = name;
        std::string license = fx::natives::cfx::GetPlayerIdentifierByType(id.c_str(), "license2");
        fx::trace("%s [%s] (%s) has connected. Players Online: %zu\n", name.c_str(), license.c_str(), id.c_str(), players->size());
    });

    fx::on("playerDropped", [players](const std::string& source, std::string reason) {
        std::string id = extractId(source);
        std::string name = players->count(id) ? (*players)[id] : "Unknown";
        std::string license = fx::natives::cfx::GetPlayerIdentifierByType(id.c_str(), "license2");
        players->erase(id);
        fx::trace("%s [%s] (%s) has disconnected (%s). Players Online: %zu\n", name.c_str(), license.c_str(), id.c_str(), reason.c_str(), players->size());
    });

    fx::onNet("chatMessage", [players](const std::string& source, int author, std::string name, std::string message) {
        fx::trace("%s (%d) has sent a chat message: %s\n", name.c_str(), author, message.c_str());
    });

    fx::addStateBagChangeHandler("", "", [](const std::string& bagName, const std::string& key, const fx::json::Value& value, int source, bool replicated) {
        fx::trace("[fx::addStateBagChangeHandler] %s:%s changed (source=%d, replicated=%s)\n", bagName.c_str(), key.c_str(), source, replicated ? "true" : "false");
    });

    fx::addExport("getPlayerInfo", [](fx::EventArgs args) -> fx::json::Value {
        if (args.size() == 0) return "unknown";
        std::string src = std::to_string(args.get<int>(0));
        std::string name = fx::natives::cfx::GetPlayerName(src.c_str());
        int ping = fx::natives::cfx::GetPlayerPing(src.c_str());
        return name + " (ping=" + std::to_string(ping) + "ms)";
    });

    fx::onCommand("c++", [](const std::string& source, const std::vector<std::string>&) {
        if (source.empty() || source == "0") return;
        std::string name = fx::getCurrentResourceName();
        fx::json::Value info = fx::callExport(name, "getPlayerInfo", {std::stoi(source)});
        fx::trace("[fx::addExport] %s\n", info.asStr("failed").c_str());
        fx::callExport("fwa", "SendChatMessage", {std::stoi(source), "^#f0a0e4[INFO] ^#ffffffHello from ^#f0a0e4C++^#ffffff!"});
    });

    fx::createThread([]() -> fx::ScriptTask {
        fx::trace("[fx::createThread] started\n");
        for (int i = 1; i <= 5; i++)
        {
            co_await fx::Wait{2000};
            fx::trace("[fx::createThread] tick %d (every 2s)\n", i);
        }
        fx::trace("[fx::createThread] done\n");
    });
}
