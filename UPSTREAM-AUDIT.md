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

### Required Animus insert safety correction

Disassembly of `CRFWorldDatabase::Insert_AnimusData` at `0x1404A0130` proves
that it reads six consecutive `double` values (`Data0` through `Data5`). The
checksum object has only `m_dValues[2]`. Upstream passes that two-value member
directly, causing four out-of-bounds reads. The original ZoneServer
`InsertTrunkData` path instead creates a zeroed six-value buffer, confirming the
callee contract.

The standalone fallback therefore creates six zeroed values and maps the
current pair to indices `race * 2` and `race * 2 + 1` before calling the same
upstream insert address. It then performs the same Animus update retry. This is
the only deliberate gameplay-path deviation and prevents a newly inserted row
from receiving stack garbage.

The preferred inline deployment embeds that same corrected sequence directly
inside the audited ZoneServer function. Its payload and every modified PE
offset are defined and verified by `Patch-ZoneServer-Inline.ps1`.

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

## DLL-free inline boundary

The inline patcher accepts only the original executable with SHA-256
`BBA474712FB58036CDEF8D387FB9C8AFDC89925323C8743374B15C018E04A545` and
never overwrites it. It deterministically produces SHA-256
`1CE090402FD705DA0B1ADEA271F26F3A0DFDE9B8F471E3CA22615217886611A7`.

The replacement body remains inside the original runtime-function range
`RVA 0x2C0B60..0x2C0BF3`. Its stack allocation grows from `0x20` to `0x50`
for the six-value Animus buffer. The x64 unwind record changes from
`01 28 02 00 0F 32 0B 70` to `01 0F 02 00 0F 92 0B 70`, exactly describing
the new prologue.

One `.npcfix` section is appended at RVA `0x44AFE000`, raw offset `0x9F8E00`.
It is read+execute code, not writable, and contains only four stack-neutral
tail-call stubs to the audited database addresses. No import, entry point, data
directory, dependency, or configuration string changes. The patcher verifies
that all existing bytes outside the audited PE metadata, target function, and
two unwind bytes remain unchanged.

The inline mode has no DLL, MinHook, `DllMain`, loader hook, configuration file,
or runtime dependency. No ZoneServer binary is committed or uploaded; the user
applies the patcher to their own verified copy.

## DLL compile boundary

The DLL project contains seven audited translation units. The diagnostic source
is inert unless its build flag is enabled:

- `src/NpcDataFallback.cpp`
- `src/NpcDataFix.cpp`
- `src/RuntimeDiagnostics.cpp`
- `third_party/minhook/src/buffer.c`
- `third_party/minhook/src/hook.c`
- `third_party/minhook/src/trampoline.c`
- `third_party/minhook/src/hde/hde64.c`

The optional diagnostic build enables the runtime observer. It writes a bounded
Win32 log but does not change parameters, branches, or results.

It contains no `ProjectReference`, no `YorozuyaGSLib`, no ATF registry, no
Yorozuya module registry, and no source for anti-dupe, combat, GM, speedhack,
auction, mail, trade, or another gameplay fix. CI parses the project and fails
if this audited compile graph changes.

`NpcDataFixTests.vcxproj` is a separate test executable. Its scenarios
verify success, insert failure, and retry failure for both fallback sequences;
all three race mappings, invalid-race rejection, and observer ordering;
it is not linked into the DLL or shipped in the installation artifact.

## DLL loader boundary

The upstream `RFOnline addon for server.zip` contains only a Visual Studio addon
template; it is not a loader. The upstream v1.4.2 `ZoneServerUD_x64.exe` forces
load-time loading through the import `YorozuyaGS.dll!YorozuyaGS`. Consequently,
the standalone binary retains that exact filename and its single exact export.
No proprietary ZoneServer binary is redistributed by this repository.

Before installing the hook, the optional DLL deployment checks the audited
ZoneServer image base, x64 PE machine, linker timestamp (`0x4A7BAF5B`), image
size (`0x44AFF000`), executable target mapping, and the first 16 upstream bytes
of the hooked function. This prevents the five absolute addresses from being
used on an unrecognized executable that merely happens to have the same
preferred base.
