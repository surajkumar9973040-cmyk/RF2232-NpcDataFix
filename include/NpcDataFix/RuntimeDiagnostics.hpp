#pragma once

#include <Windows.h>

#include <cstdint>

#include <NpcDataFix/NpcDataFallback.hpp>

namespace NpcDataFix::RuntimeDiagnostics
{
    struct CallContext
    {
        std::uint64_t id;
        bool enabled;
    };

    void SetModule(HMODULE module) noexcept;

    CallContext BeginCall(
        const void* data,
        const void* database,
        DWORD installStatus) noexcept;

    void WriteIdentity(
        const CallContext& call,
        std::uint32_t characterSerial,
        std::uint32_t accountSerial,
        char race) noexcept;

    void __cdecl BeforeOperation(
        void* context,
        FallbackOperation operation) noexcept;

    void __cdecl AfterOperation(
        void* context,
        FallbackOperation operation,
        bool result) noexcept;

    void EndCall(const CallContext& call, bool result) noexcept;
}
