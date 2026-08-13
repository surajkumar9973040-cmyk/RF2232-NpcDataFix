# RF2232-NpcDataFix

Minimal standalone x64 DLL containing only goodwinxp/Yorozuya's
`Fixed character rollback via NpcData` hook for RF Online Server 2.2.3.2.

## Scope

The DLL hooks `CCheckSumCharacAccountTrunkData::Update` at the exact upstream
address and preserves both fallback sequences:

1. `Update_NpcData` -> if false, `Insert_NpcData` -> retry `Update_NpcData`.
2. `Update_AnimusData` -> if false, `Insert_AnimusData` -> retry
   `Update_AnimusData`.

It does not compile or link YorozuyaGSLib, ATF Registry, ModuleRegistry, or any
other Yorozuya gameplay fix. The complete DLL compile graph is two local C++
files plus four MinHook C files.

## Build

Requirements: Visual Studio 2022 with the Desktop development with C++ workload.

```powershell
git clone --recurse-submodules https://github.com/surajkumar9973040-cmyk/RF2232-NpcDataFix.git
cd RF2232-NpcDataFix
msbuild .\RF2232-NpcDataFix.sln /m /t:Rebuild /p:Configuration=Release /p:Platform=x64
.\build\x64\Release\NpcDataFixTests.exe
```

Output: `build\x64\Release\YorozuyaGS.dll` (x64, `/MT`, v143).

The GitHub Actions workflow publishes the installable `RF2232-NpcDataFix`
artifact. See `README-INSTALL.txt` inside it before changing a server.

## Audited upstream

- `goodwinxp/Yorozuya` commit
  `ad0f376c169a3875692bd080b689d14c018dcb5d` (`v1.4.2`).
- NpcData fix commit: `af1964f86e1f1df9a8915c86a890e8e8c400d24f`.
- MinHook: official `v1.3.4`, pinned as a git submodule at
  `c3fcafdc10146beb5919319d0683e44e3c30d537`.

Exact addresses and source mapping are documented in
[`UPSTREAM-AUDIT.md`](UPSTREAM-AUDIT.md). Installation and rollback instructions
are in [`README-INSTALL.txt`](README-INSTALL.txt).
