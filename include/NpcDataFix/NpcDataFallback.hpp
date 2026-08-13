#pragma once

#include <cstddef>
#include <cstdint>

namespace NpcDataFix
{
    // Layout copied from library/ATF/CCheckSumCharacAccountTrunkData.hpp.
    // RF Online 2.2.3.2 x64 and MSVC both use an 8-byte long double.
#pragma pack(push, 8)
    struct CCheckSumCharacAccountTrunkData
    {
        std::uint32_t m_dwSerial;
        std::uint32_t m_dwAccountSerial;
        char m_byRace;
        std::uint32_t m_dwValues[6];
        long double m_dValues[2];
    };
#pragma pack(pop)

    struct CRFWorldDatabase;

    using UpdateNpcDataFn = bool(__cdecl*)(
        CRFWorldDatabase*, std::uint32_t, std::uint32_t*);
    using InsertNpcDataFn = bool(__cdecl*)(
        CRFWorldDatabase*, std::uint32_t, std::uint32_t*);
    using UpdateAnimusDataFn = bool(__cdecl*)(
        CRFWorldDatabase*, std::uint32_t, char, long double*);
    using InsertAnimusDataFn = bool(__cdecl*)(
        CRFWorldDatabase*, std::uint32_t, long double*);

    struct DatabaseApi
    {
        UpdateNpcDataFn updateNpcData;
        InsertNpcDataFn insertNpcData;
        UpdateAnimusDataFn updateAnimusData;
        InsertAnimusDataFn insertAnimusData;
    };

    // Implements YorozuyaGS/NpcData/NpcData.cpp without the ATF wrapper.
    [[nodiscard]] bool ApplyNpcDataFallback(
        CCheckSumCharacAccountTrunkData* data,
        CRFWorldDatabase* database,
        const DatabaseApi& api);

    static_assert(sizeof(void*) == 8, "RF 2.2.3.2 fix must be built for x64");
    static_assert(sizeof(long double) == 8, "RF 2.2.3.2 x64 requires MSVC's 8-byte long double");
    static_assert(sizeof(CCheckSumCharacAccountTrunkData) == 56, "Unexpected checksum data layout");
    static_assert(offsetof(CCheckSumCharacAccountTrunkData, m_dwSerial) == 0);
    static_assert(offsetof(CCheckSumCharacAccountTrunkData, m_dwAccountSerial) == 4);
    static_assert(offsetof(CCheckSumCharacAccountTrunkData, m_byRace) == 8);
    static_assert(offsetof(CCheckSumCharacAccountTrunkData, m_dwValues) == 12);
    static_assert(offsetof(CCheckSumCharacAccountTrunkData, m_dValues) == 40);
}
