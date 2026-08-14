RF Online Server 2.2.3.2 x64 - DLL-free NpcData fix
====================================================

ЭТОТ ВАРИАНТ НЕ ИСПОЛЬЗУЕТ YOROZUYAGS.DLL И MINHOOK

Исправление встраивается непосредственно в копию вашего родного
ZoneServerUD_x64.exe. Оно исполняется только при сохранении данных персонажа,
поэтому при запуске сервера нет DllMain, loader hook или сторонней DLL.

ПОДДЕРЖИВАЕМЫЙ ОРИГИНАЛ

  Имя:     ZoneServerUD_x64.exe
  Размер:  10 456 576 байт
  SHA-256: BBA474712FB58036CDEF8D387FB9C8AFDC89925323C8743374B15C018E04A545

Патчер откажется работать с любым другим EXE и никогда не перезаписывает
исходный файл.

СОЗДАНИЕ ПРОПАТЧЕННОЙ КОПИИ

1. Поместите рядом:

     ZoneServerUD_x64.exe
     Patch-ZoneServer-Inline.ps1

2. Откройте PowerShell в этой папке и выполните:

     powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Patch-ZoneServer-Inline.ps1 -InputPath .\ZoneServerUD_x64.exe

3. Будет создан отдельный файл:

     ZoneServerUD_x64.NpcDataFix.exe

   Ожидаемые параметры результата:

     Размер:  10 457 088 байт
     SHA-256: 1CE090402FD705DA0B1ADEA271F26F3A0DFDE9B8F471E3CA22615217886611A7

УСТАНОВКА НА ВИРТУАЛЬНУЮ МАШИНУ

Целевая папка пользователя:

  C:\RF2232\Server\2_ZoneServer\RF_Bin

1. Полностью остановите ZoneServer.
2. Сделайте backup базы RF_World.
3. Переименуйте текущий файл:

     ZoneServerUD_x64.exe -> ZoneServerUD_x64.original.backup.exe

4. Скопируйте ZoneServerUD_x64.NpcDataFix.exe в эту папку и переименуйте:

     ZoneServerUD_x64.NpcDataFix.exe -> ZoneServerUD_x64.exe

5. Уберите оставшийся от прежних тестов YorozuyaGS.dll, например:

     YorozuyaGS.dll -> YorozuyaGS.dll.off

   DLL-free EXE не импортирует и не загружает YorozuyaGS.dll.
6. Запустите ZoneServer обычным способом.

ПРОВЕРКА

Проверьте установленный EXE:

  Get-FileHash 'C:\RF2232\Server\2_ZoneServer\RF_Bin\ZoneServerUD_x64.exe' -Algorithm SHA256

Ожидается:

  1CE090402FD705DA0B1ADEA271F26F3A0DFDE9B8F471E3CA22615217886611A7

Во время работы сервера следующая команда не должна выводить YorozuyaGS.dll:

  (Get-Process ZoneServerUD_x64).Modules |
    Where-Object ModuleName -eq 'YorozuyaGS.dll'

Игровой тест выполняйте только на новом персонаже, созданном после установки:

1. Создать персонажа.
2. Войти в игру.
3. Выйти.
4. Снова войти тем же персонажем.
5. Повторить logout/relogin 2-3 раза.

Ранее повреждённый персонаж автоматически не восстанавливается. Не удаляйте
строки tbl_NpcData без отдельного backup базы.

ОТКАТ

1. Остановите ZoneServer.
2. Удалите или переименуйте пропатченный ZoneServerUD_x64.exe.
3. Верните backup:

     ZoneServerUD_x64.original.backup.exe -> ZoneServerUD_x64.exe

4. Запустите сервер.

ЧТО ИЗМЕНЕНО

- imports, entry point и все прежние зависимости сохранены;
- YorozuyaGS.dll не добавляется;
- DB/network строки и игровые конфиги исходного EXE не заменяются;
- добавлена одна read+execute секция .npcfix размером 512 байт, без write;
- старое тело CCheckSumCharacAccountTrunkData::Update заменено узкой логикой:

    Update_NpcData
      -> при false: Insert_NpcData -> повторный Update_NpcData

    Update_AnimusData
      -> при false: Insert_AnimusData -> повторный Update_AnimusData

- для Insert_AnimusData создаётся безопасный массив из шести double. Upstream
  передавал только два значения, хотя серверная функция читает шесть.

Использованные адреса RF Online Server 2.2.3.2 x64:

  CCheckSumCharacAccountTrunkData::Update  0x1402C0B60
  CRFWorldDatabase::Insert_NpcData         0x14049EF50
  CRFWorldDatabase::Update_NpcData         0x14049F3C0
  CRFWorldDatabase::Insert_AnimusData      0x1404A0130
  CRFWorldDatabase::Update_AnimusData      0x1404A0260

Никакие anti-dupe, combat, GM, speedhack, auction, mail, trade или другие
фиксы Yorozuya в EXE не добавляются.

Если новый crash происходит ДО первого logout, сохраните новый exception log:
inline-код к этому моменту ещё не выполнялся. Если crash возникает именно на
logout, приложите exception log, ZoneServer DB/System logs и SHA-256 запущенного
EXE.
