#pragma once

#include <cstdint>

namespace NpcDataFix::UpstreamAddresses
{
    // goodwinxp/Yorozuya commit ad0f376c169a3875692bd080b689d14c018dcb5d,
    // generated for RF Online Server 2.2.3.2 x64.
    inline constexpr std::uintptr_t ZoneServerImageBase = 0x140000000ULL;
    inline constexpr std::uint32_t ZoneServerTimeDateStamp = 0x4A7BAF5BU;
    inline constexpr std::uint32_t ZoneServerSizeOfImage = 0x44AFF000U;
    inline constexpr std::uintptr_t CCheckSumCharacAccountTrunkData_Update = 0x1402C0B60ULL;
    inline constexpr std::uintptr_t CRFWorldDatabase_InsertNpcData = 0x14049EF50ULL;
    inline constexpr std::uintptr_t CRFWorldDatabase_UpdateNpcData = 0x14049F3C0ULL;
    inline constexpr std::uintptr_t CRFWorldDatabase_InsertAnimusData = 0x1404A0130ULL;
    inline constexpr std::uintptr_t CRFWorldDatabase_UpdateAnimusData = 0x1404A0260ULL;
}
