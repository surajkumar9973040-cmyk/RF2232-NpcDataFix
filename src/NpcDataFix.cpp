#include <Windows.h>
#include <MinHook.h>

#include <cstdint>

#include <NpcDataFix/NpcDataFallback.hpp>
#if defined(NPCDATAFIX_DIAGNOSTICS)
#include <NpcDataFix/RuntimeDiagnostics.hpp>
#endif
#include <NpcDataFix/UpstreamAddresses.hpp>

namespace
{
    using namespace NpcDataFix;

    DWORD g_status = 0;

    template <typename T>
    T At(const std::uintptr_t address) noexcept
    {
        return reinterpret_cast<T>(address);
    }

    const DatabaseApi g_databaseApi{
        At<UpdateNpcDataFn>(UpstreamAddresses::CRFWorldDatabase_UpdateNpcData),
        At<InsertNpcDataFn>(UpstreamAddresses::CRFWorldDatabase_InsertNpcData),
        At<UpdateAnimusDataFn>(UpstreamAddresses::CRFWorldDatabase_UpdateAnimusData),
        At<InsertAnimusDataFn>(UpstreamAddresses::CRFWorldDatabase_InsertAnimusData),
    };

    constexpr unsigned char ExpectedUpdatePrologue[]{
        0x48, 0x89, 0x54, 0x24, 0x10, 0x48, 0x89, 0x4C,
        0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48,
    };

    bool HasExpectedUpdatePrologue(const void* const target) noexcept
    {
        const auto* actual = static_cast<const unsigned char*>(target);
        for (std::size_t index = 0; index < sizeof(ExpectedUpdatePrologue); ++index)
        {
            if (actual[index] != ExpectedUpdatePrologue[index])
            {
                return false;
            }
        }
        return true;
    }

    bool __cdecl UpdateDetour(
        CCheckSumCharacAccountTrunkData* const data,
        CRFWorldDatabase* const database)
    {
        // The upstream callback receives a trampoline but intentionally never calls it.
#if defined(NPCDATAFIX_DIAGNOSTICS)
        auto diagnostics = RuntimeDiagnostics::BeginCall(data, database, g_status);
        if (data != nullptr)
        {
            RuntimeDiagnostics::WriteIdentity(
                diagnostics, data->m_dwSerial, data->m_dwAccountSerial, data->m_byRace);
        }
        const FallbackObserver observer{
            &diagnostics,
            RuntimeDiagnostics::BeforeOperation,
            RuntimeDiagnostics::AfterOperation,
        };
        const bool result = ApplyNpcDataFallback(data, database, g_databaseApi, &observer);
        RuntimeDiagnostics::EndCall(diagnostics, result);
        return result;
#else
        return ApplyNpcDataFallback(data, database, g_databaseApi);
#endif
    }

    bool IsExpectedTarget() noexcept
    {
        const auto mainModule = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        if (mainModule != UpstreamAddresses::ZoneServerImageBase)
        {
            return false;
        }

        const auto* dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(mainModule);
        if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE || dosHeader->e_lfanew <= 0)
        {
            return false;
        }

        const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            mainModule + static_cast<std::uintptr_t>(dosHeader->e_lfanew));
        if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
            ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            ntHeaders->FileHeader.TimeDateStamp != UpstreamAddresses::ZoneServerTimeDateStamp ||
            ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            ntHeaders->OptionalHeader.SizeOfImage != UpstreamAddresses::ZoneServerSizeOfImage)
        {
            return false;
        }

        const auto target = At<void*>(
            UpstreamAddresses::CCheckSumCharacAccountTrunkData_Update);

        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(target, &memory, sizeof(memory)) != sizeof(memory))
        {
            return false;
        }

        const DWORD executable = PAGE_EXECUTE | PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
        return memory.State == MEM_COMMIT &&
            memory.Type == MEM_IMAGE &&
            (memory.Protect & executable) != 0 &&
            memory.AllocationBase == reinterpret_cast<void*>(mainModule) &&
            HasExpectedUpdatePrologue(target);
    }

    bool InstallHook() noexcept
    {
        if (!IsExpectedTarget())
        {
            g_status = 1;
            OutputDebugStringW(
                L"RF2232-NpcDataFix: unexpected image base or target address.\n");
            return false;
        }

        MH_STATUS status = MH_Initialize();
        if (status != MH_OK)
        {
            g_status = 2;
            return false;
        }

        const auto target = At<void*>(
            UpstreamAddresses::CCheckSumCharacAccountTrunkData_Update);
        status = MH_CreateHook(
            target,
            reinterpret_cast<void*>(&UpdateDetour),
            nullptr);
        if (status != MH_OK)
        {
            g_status = 3;
            MH_Uninitialize();
            return false;
        }

        status = MH_EnableHook(target);
        if (status != MH_OK)
        {
            g_status = 4;
            MH_RemoveHook(target);
            MH_Uninitialize();
            return false;
        }

        g_status = 5;
        return true;
    }

    void RemoveHook() noexcept
    {
        const auto target = At<void*>(
            UpstreamAddresses::CCheckSumCharacAccountTrunkData_Update);
        MH_DisableHook(target);
        MH_RemoveHook(target);
        MH_Uninitialize();
        g_status = 0;
    }
}

extern "C" __declspec(dllexport) void __cdecl YorozuyaGS() noexcept
{
    // Compatibility export used by the original Yorozuya-enabled ZoneServer.
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        // The upstream YorozuyaGS also installs its ATF hooks during process attach.
        // Static CRT is used, so DisableThreadLibraryCalls is intentionally omitted.
#if defined(NPCDATAFIX_DIAGNOSTICS)
        RuntimeDiagnostics::SetModule(module);
#endif
        return InstallHook() ? TRUE : FALSE;

    case DLL_PROCESS_DETACH:
        // On normal process termination Windows will reclaim all mappings. Avoid
        // suspending other terminating threads from MinHook under the loader lock.
        if (reserved == nullptr && g_status == 5)
        {
            RemoveHook();
        }
        break;

    default:
        break;
    }

    UNREFERENCED_PARAMETER(module);
    return TRUE;
}
