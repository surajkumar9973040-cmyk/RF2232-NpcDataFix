RF Online Server 2.2.3.2 x64 - standalone NpcData rollback fix
================================================================

WHAT THIS PACKAGE CONTAINS

  YorozuyaGS.dll        The standalone fix (x64 Release, static MSVC runtime).
  diagnostic\            A test-only DLL with a bounded runtime operation log.
  SHA256SUMS.txt        SHA-256 checksum for the DLL.
  LICENSE-*.txt         Upstream Yorozuya and MinHook license notices.

There is no config file. There are no addon DLLs and no Visual C++ runtime DLLs.
Only the NpcData/AnimusData fallback from goodwinxp/Yorozuya is included. The
Animus insert uses a six-value buffer because the target function reads six
values; passing upstream's two-value member would read beyond the object.

IMPORTANT COMPATIBILITY REQUIREMENT

This DLL is loaded by a ZoneServerUD_x64.exe that imports:

  YorozuyaGS.dll!YorozuyaGS

The official goodwinxp/Yorozuya v1.4.2 server release uses that import. An
unmodified RF Online ZoneServer does not load this DLL by itself. This package
does not contain ZoneServerUD_x64.exe because that server binary is not part of
this source repository and must not be invented or redistributed here.

Reference upstream release:
  https://github.com/goodwinxp/Yorozuya/releases/tag/v1.4.2

The import-patched ZoneServerUD_x64.exe in that release has:
  Size:    10,460,160 bytes
  SHA-256: EA854DC7E9BA09490316B9FEFC6BFD89EB965BFDE79B7912D4C915E6AE646F59

Do NOT replace your server EXE with that upstream binary merely to obtain the
import. It also contains unrelated database/network configuration changes.
Use an import-only patch made from your own backed-up executable.

The upstream file "RFOnline addon for server.zip" is only a Visual Studio addon
project template. It is not a loader and is intentionally not included.

BEFORE INSTALLING

1. Stop ZoneServer completely.
2. Back up the game database (the acceptance test creates character data).
3. Back up these files from the server directory:

     ZoneServerUD_x64.exe
     YorozuyaGS.dll             (if it already exists)
     YorozuyaGS\                (the whole folder, if it already exists)

4. Do not combine this DLL with full YorozuyaGS. It replaces the main
   YorozuyaGS.dll and cannot coexist with the full core under the same name.
5. Optionally verify that your ZoneServer expects the DLL from a Visual Studio
   Developer Command Prompt:

     dumpbin /dependents ZoneServerUD_x64.exe | findstr /i YorozuyaGS.dll

   If no YorozuyaGS.dll import is shown, stop: the DLL alone will not load.

INSTALL

1. Copy YorozuyaGS.dll next to ZoneServerUD_x64.exe. Example layout:

     Server\ZoneServerUD_x64.exe
     Server\YorozuyaGS.dll

   Do NOT copy this standalone DLL into the Server\YorozuyaGS\ addon folder.
2. No global.json or other config is required.
3. Start ZoneServer normally and watch its normal startup/crash logs.

If the acceptance test still fails, stop ZoneServer and temporarily replace
the installed DLL with diagnostic\YorozuyaGS.dll from this package. It has the
same game logic but writes at most the first 256 hook calls to:

     NpcDataFix-runtime.log

The log is created next to YorozuyaGS.dll on the first hooked save. It contains
only serial/account/race IDs and operation results, not passwords or DB values.

VERIFY THAT THE DLL IS LOADED

Run 64-bit PowerShell as Administrator while ZoneServer is running:

  (Get-Process ZoneServerUD_x64).Modules |
    Where-Object ModuleName -eq 'YorozuyaGS.dll'

The command must list YorozuyaGS.dll from the same directory as ZoneServer.
If the DLL does not match RF Online Server 2.2.3.2 x64, it deliberately refuses
to load instead of installing a hook at an unverified address.

ACCEPTANCE TEST

1. Start the server.
2. Create a new character.
3. Enter the game with that character.
4. Log out.
5. Enter the game again with THE SAME character.

Success means the same character enters after logout/relogin. Also check normal
ZoneServer and database logs for failed NpcData or AnimusData operations.

Use a character created AFTER the fixed DLL is confirmed loaded. A checksum row
already corrupted before installation is not automatically repaired.

DIAGNOSTIC LOG

For each operation, result=1 means success and result=0 means failure. Expected
fallback order is:

  npc.update.initial
  npc.insert             (only when the initial update returned 0)
  npc.update.retry       (only when insert returned 1)
  animus.update.initial
  animus.insert          (only when the initial update returned 0)
  animus.update.retry    (only when insert returned 1)

event=exit result=1 means the complete hook succeeded. If the log is not
created after logout, the tested process did not execute this hook (commonly a
different EXE/DLL path). If every stage succeeds but the client still closes,
the fault is outside this NpcData hook. Send NpcDataFix-runtime.log together
with the normal ZoneServer/DB log from the same test.

REMOVE / ROLLBACK

1. Stop ZoneServer completely.
2. Restore the backed-up original ZoneServerUD_x64.exe FIRST.
3. Restore the previous YorozuyaGS.dll and YorozuyaGS\ folder if they existed;
   otherwise remove this package's YorozuyaGS.dll.
   You may also remove NpcDataFix-runtime.log; it contains diagnostics only.
4. Start ZoneServer and verify normal startup.

Do not simply delete YorozuyaGS.dll while keeping an import-patched ZoneServer:
Windows will refuse to start an executable whose required imported DLL is gone.

SCOPE AND VERSION

Target: RF Online Server 2.2.3.2 x64, image base 0x140000000.
Upstream: goodwinxp/Yorozuya commit
          ad0f376c169a3875692bd080b689d14c018dcb5d (tag v1.4.2).

Hook:
  CCheckSumCharacAccountTrunkData::Update       0x1402C0B60

Database calls:
  CRFWorldDatabase::Insert_NpcData(values)     0x14049EF50
  CRFWorldDatabase::Update_NpcData             0x14049F3C0
  CRFWorldDatabase::Insert_AnimusData           0x1404A0130
  CRFWorldDatabase::Update_AnimusData           0x1404A0260

No anti-dupe, combat, GM, speedhack, auction, mail, trade, or other Yorozuya
game fixes are compiled into this DLL.

Known limitation: hook installation and explicit-unload cleanup run from
DllMain, matching upstream YorozuyaGS. MinHook suspends/resumes process threads
while enabling a hook, so this retains upstream's loader-lock risk. Do not
hot-load or hot-unload the DLL in a running server; install/remove only while
ZoneServer is stopped.
