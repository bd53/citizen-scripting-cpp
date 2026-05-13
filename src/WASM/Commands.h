#pragma once

#include "Imports.h"
#include "Types.h"
#include "Context.h"
#include "Refs.h"
#include "Natives.h"

namespace fx
{

inline void onCommand(const std::string& command, CommandHandler h)
{
    if (auto* c = fxw_internal::currentContext())
        c->commands[command].push_back(h);
    int32_t hostRef = detail::createRef([command](const uint8_t* args, uint32_t argsSize) -> std::vector<uint8_t> {
        auto decoded = fxw_internal::decode(args, argsSize);
        fxw_internal::ensureArray(decoded);
        std::string source = decoded.size() > 0 ? std::to_string(decoded.at(0).asInt()) : "0";
        std::vector<std::string> cmdArgs;
        if (decoded.size() > 1 && decoded.at(1).kind == fxw_internal::Value::Kind::Array)
            for (size_t i = 0; i < decoded.at(1).size(); ++i)
                cmdArgs.push_back(decoded.at(1).at(i).asStr());
        auto* ctx = fxw_internal::currentContext();
        if (ctx)
        {
            auto it = ctx->commands.find(command);
            if (it != ctx->commands.end())
                for (auto& handler : it->second) handler(source, cmdArgs);
        }
        return { 0x90 };
    });
    std::string cbRef = detail::canonicalizeRef(hostRef);
    if (cbRef.empty()) { detail::removeRef(hostRef); return; }
    invokeNative(HashString("REGISTER_COMMAND"), {NativeArg::ptr(command.c_str()), NativeArg::ptr(cbRef.c_str()), NativeArg(0)});
}

}
