#pragma once

#include "Imports.h"
#include "Types.h"
#include "Context.h"
#include "Refs.h"
#include "Events.h"
#include "Natives.h"
#include "Metadata.h"

namespace fx
{

inline void addExport(const std::string& name, ExportHandler handler)
{
    int32_t hostRef = detail::createRef([handler](const uint8_t* args, uint32_t argsSize) -> std::vector<uint8_t> {
        auto decoded = fxw_internal::decode(args, argsSize);
        fxw_internal::ensureArray(decoded);
        EventArgs ea(std::move(decoded));
        json::Value result = handler(ea);
        fxw_internal::Value arr;
        arr.kind = fxw_internal::Value::Kind::Array;
        arr.children.push_back(std::move(result));
        return fxw_internal::encode(arr);
    });
    std::string exportRef = detail::canonicalizeRef(hostRef);
    if (exportRef.empty()) { detail::removeRef(hostRef); return; }
    std::string resName = getCurrentResourceName();
    std::string eventName = "__cfx_export_" + resName + "_" + name;
    on(eventName, [exportRef](const std::string&, EventArgs args) {
        if (args.size() == 0) return;
        std::string setterRef = args.funcRef(0);
        if (setterRef.empty()) return;
        fxw_internal::Value refVal;
        refVal.kind = fxw_internal::Value::Kind::FuncRef;
        refVal.scalar = exportRef;
        fxw_internal::Value arr;
        arr.kind = fxw_internal::Value::Kind::Array;
        arr.children.push_back(std::move(refVal));
        auto payload = fxw_internal::encode(arr);
        detail::invokeFunctionReference(setterRef, payload.data(), static_cast<uint32_t>(payload.size()));
    });
}

inline json::Value callExport(const std::string& resource, const std::string& name, std::initializer_list<json::Value> args = {})
{
    auto capturedRef = std::make_shared<std::string>();
    int32_t setterRef = detail::createRef([capturedRef](const uint8_t* data, uint32_t size) -> std::vector<uint8_t> {
        auto decoded = fxw_internal::decode(data, size);
        if (decoded.kind == fxw_internal::Value::Kind::FuncRef)
            *capturedRef = decoded.scalar;
        else if (decoded.kind == fxw_internal::Value::Kind::Array && decoded.size() > 0 && decoded.at(0).kind == fxw_internal::Value::Kind::FuncRef)
            *capturedRef = decoded.at(0).scalar;
        return { 0x90 };
    });
    std::string setterRefStr = detail::canonicalizeRef(setterRef);
    if (setterRefStr.empty()) return {};
    fxw_internal::Value setterVal;
    setterVal.kind = fxw_internal::Value::Kind::FuncRef;
    setterVal.scalar = setterRefStr;
    fxw_internal::Value setterArr;
    setterArr.kind = fxw_internal::Value::Kind::Array;
    setterArr.children.push_back(std::move(setterVal));
    auto setterPayload = fxw_internal::encode(setterArr);
    std::string eventName = "__cfx_export_" + resource + "_" + name;
    fxw_internal::Writer w;
    w.arrayHeader(static_cast<uint32_t>(setterPayload.size()));
    __fxcpp_emit_event(eventName.c_str(), static_cast<uint32_t>(eventName.size()), setterPayload.data(), static_cast<uint32_t>(setterPayload.size()));
    detail::removeRef(setterRef);
    if (capturedRef->empty()) return {};
    fxw_internal::Value argArr;
    argArr.kind = fxw_internal::Value::Kind::Array;
    argArr.children.assign(args.begin(), args.end());
    auto userPayload = fxw_internal::encode(argArr);
    auto retData = detail::invokeFunctionReference(*capturedRef, userPayload.data(), static_cast<uint32_t>(userPayload.size()));
    if (retData.empty()) return {};
    auto result = fxw_internal::decode(retData.data(), static_cast<uint32_t>(retData.size()));
    if (result.kind == fxw_internal::Value::Kind::Array && result.size() > 0)
        return result.at(0);
    return result;
}

}
