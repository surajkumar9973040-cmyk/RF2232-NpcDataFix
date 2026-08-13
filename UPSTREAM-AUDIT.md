# Upstream audit and dependency boundary

The behavior and addresses in this repository were audited against
`goodwinxp/Yorozuya` commit
`ad0f376c169a3875692bd080b689d14c018dcb5d` (`v1.4.2`). The original fix was
introduced by upstream commit `af1964f86e1f1df9a8915c86a890e8e8c400d24f`.

## Behavior mapping

| Upstream behavior | Upstream source | Standalone implementation |
|---|---|---|
| Hook `CCheckSumCharacAccountTrunkData::Update` | `YorozuyaGS/NpcData/NpcData.cpp` | `src/NpcDataFix.cpp::UpdateDetour` |
| Update/insert/retry NpcData | same file | `src/NpcDataFallback.cpp` |
| Update/insert/retry AnimusData | same file | `src/NpcDataFallback.cpp` |
| 56-byte object layout | `library/ATF/CCheckSumCharacAccountTrunkData.hpp` | `include/NpcDataFix/NpcDataFallback.hpp` |
| Empty loader anchor export `YorozuyaGS` | `YorozuyaGS/Yorozuya.cpp` | `src/NpcDataFix.cpp` |

The generated ATF callback has a third `next` argument, but upstream never calls
it. That argument is supplied only by the ATF wrapper. A direct MinHook detour
therefore uses the actual two-argument x64 function ABI: object pointer and
database pointer.

## Exact upstream addresses

| Symbol | Address | Upstream generated source |
|---|---:|---|
| `CCheckSumCharacAccountTrunkData::Update` | `0x1402C0B60` | `YorozuyaGSLib/source/CCheckSumCharacAccountTrunkData.cpp` and `CCheckSumCharacAccountTrunkDataDetail.cpp` |
| `CRFWorldDatabase::Insert_NpcData(serial, values)` | `0x14049EF50` | `YorozuyaGSLib/source/CRFWorldDatabase.cpp` and `CRFWorldDatabaseDetail.cpp` |
| `CRFWorldDatabase::Update_NpcData` | `0x14049F3C0` | same files |
| `CRFWorldDatabase::Insert_AnimusData` | `0x1404A0130` | same files |
| `CRFWorldDatabase::Update_AnimusData` | `0x1404A0260` | same files |

The overload `Insert_NpcData(serial)` at `0x14049EEA0` is deliberately not used;
the upstream fix calls `Insert_NpcData(serial, values)` at `0x14049EF50`.

## Compile boundary

`NpcDataFix.vcxproj` contains exactly six translation units:

- `src/NpcDataFallback.cpp`
- `src/NpcDataFix.cpp`
- `third_party/minhook/src/buffer.c`
- `third_party/minhook/src/hook.c`
- `third_party/minhook/src/trampoline.c`
- `third_party/minhook/src/hde/hde64.c`

It contains no `ProjectReference`, no `YorozuyaGSLib`, no ATF registry, no
Yorozuya module registry, and no source for anti-dupe, combat, GM, speedhack,
auction, mail, trade, or another gameplay fix. CI parses the project and fails
if this audited compile graph changes.

`NpcDataFixTests.vcxproj` is a separate test executable. Its eight scenarios
verify success, insert failure, and retry failure for both fallback sequences;
it is not linked into the DLL or shipped in the installation artifact.

## Loader boundary

The upstream `RFOnline addon for server.zip` contains only a Visual Studio addon
template; it is not a loader. The upstream v1.4.2 `ZoneServerUD_x64.exe` forces
load-time loading through the import `YorozuyaGS.dll!YorozuyaGS`. Consequently,
the standalone binary retains that exact filename and its single exact export.
No proprietary ZoneServer binary is redistributed by this repository.

Before installing the hook, the DLL checks the audited ZoneServer image base,
x64 PE machine, linker timestamp (`0x4A7BAF5B`), image size (`0x44AFF000`),
executable target mapping, and the first 16 upstream bytes of the hooked
function. This prevents the five absolute addresses from being used on an
unrecognized executable that merely happens to have the same preferred base.
