# RF2232-NpcDataFix

Minimal standalone implementation of goodwinxp/Yorozuya's
`Fixed character rollback via NpcData` for RF Online Server 2.2.3.2 x64.

The preferred deployment is now the exact-hash DLL-free patcher
[`Patch-ZoneServer-Inline.ps1`](Patch-ZoneServer-Inline.ps1). It creates a new
copy of the audited original ZoneServer and does not use a DLL, MinHook, an
import loader, or `DllMain`. The earlier standalone DLL remains available as a
diagnostic/alternative deployment.

## Scope

Both deployment modes replace `CCheckSumCharacAccountTrunkData::Update` at the
exact upstream address and preserve both fallback sequences:

1. `Update_NpcData` -> if false, `Insert_NpcData` -> retry `Update_NpcData`.
2. `Update_AnimusData` -> if false, `Insert_AnimusData` -> retry
   `Update_AnimusData`.

They do not include YorozuyaGSLib, ATF Registry, ModuleRegistry, or any other
Yorozuya gameplay fix. The inline patch keeps all original imports and config
data, adds one non-writable RX section, and needs no runtime companion file.
The production DLL compile graph remains two local C++ files plus four MinHook
C files. A separately packaged diagnostic DLL adds one local logger source and
caps logging at 256 hook calls.

The standalone implementation corrects one memory-safety defect in the
upstream Animus fallback: `Insert_AnimusData` reads six values, while upstream
passed a two-value member. The fix builds the complete zero-initialized
six-value row and maps the current race's pair before the same insert/retry.

## Build

Requirements: Visual Studio 2022 with the Desktop development with C++ workload.

```powershell
git clone --recurse-submodules https://github.com/surajkumar9973040-cmyk/RF2232-NpcDataFix.git
cd RF2232-NpcDataFix
msbuild .\RF2232-NpcDataFix.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
.\build\x64\Release\NpcDataFixTests.exe
```

Output: `build\x64\Release\YorozuyaGS.dll` (x64, `/MT`, v143).

Diagnostic output can be built with
`/p:NpcDataFixDiagnostics=true`; it is packaged under
`diagnostic\YorozuyaGS.dll` and writes `NpcDataFix-runtime.log` beside itself.

The GitHub Actions workflow publishes the installable `RF2232-NpcDataFix`
artifact. See `README-INSTALL.txt` inside it before changing a server.

## DLL-free ZoneServer patch

The patcher accepts only this exact original:

- size: `10,456,576` bytes;
- SHA-256: `BBA474712FB58036CDEF8D387FB9C8AFDC89925323C8743374B15C018E04A545`.

It never patches in place:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\Patch-ZoneServer-Inline.ps1 `
  -InputPath .\ZoneServerUD_x64.exe
```

The output `ZoneServerUD_x64.NpcDataFix.exe` has SHA-256
`1CE090402FD705DA0B1ADEA271F26F3A0DFDE9B8F471E3CA22615217886611A7`.
No ZoneServer binary is committed or published by this repository. See
[`README-INLINE.txt`](README-INLINE.txt) for backup, installation, verification,
and rollback instructions.

## Audited upstream

- `goodwinxp/Yorozuya` commit
  `ad0f376c169a3875692bd080b689d14c018dcb5d` (`v1.4.2`).
- NpcData fix commit: `af1964f86e1f1df9a8915c86a890e8e8c400d24f`.
- MinHook: official `v1.3.4`, pinned as a git submodule at
  `c3fcafdc10146beb5919319d0683e44e3c30d537`.

Exact addresses and source mapping are documented in
[`UPSTREAM-AUDIT.md`](UPSTREAM-AUDIT.md). Installation and rollback instructions
are in [`README-INSTALL.txt`](README-INSTALL.txt).
