[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string]$InputPath,

    [Parameter(Position = 1)]
    [string]$OutputPath,

    [switch]$LibraryMode
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$script:OriginalLength = 0x9F8E00
$script:PatchedLength = 0x9F9000
$script:OriginalSha256 =
    'BBA474712FB58036CDEF8D387FB9C8AFDC89925323C8743374B15C018E04A545'
$script:PatchedSha256 =
    '1CE090402FD705DA0B1ADEA271F26F3A0DFDE9B8F471E3CA22615217886611A7'

$script:TargetOffset = 0x2BFF60
$script:TargetLength = 0x93
$script:UnwindOffset = 0x8FAA70
$script:RuntimeFunctionOffset = 0x9A0248
$script:NewSectionOffset = 0x9F8E00

$script:OriginalTargetHex = @'
488954241048894c2408574883ec20488bfc48b90800000000000000b8cccccccc
f3ab488b4c243048837c243800750432c0eb59488b4424304883c00c4c8bc0488b
4424308b10488b4c2438e88d7ad4ff0fb6c085c0742f488b4424304883c0284c8b
c8488b442430440fb64008488b4424308b5004488b4c2438e82413d5ff0fb6c085
c0750432c0eb02b0014883c4205fc3
'@

$script:PatchedTargetHex = @'
488954241048894c2408574883ec504889cf4885d27460e884d4834484c07512e88b
d4834484c0744ee872d4834484c07445e889d4834484c0753e0fb647083c0277346
60fefc0f30f7f442420f30f7f442430f30f7f442440c1e004f30f6f4728f30f7f
440420e86ad4834484c07407e84bd48344eb0231c04883c4505fc3
'@

$script:OriginalUnwindHex = '012802000f320b70'
$script:PatchedUnwindHex = '010f02000f920b70'
$script:RuntimeFunctionHex = '600b2c00f30b2c0070b68f00'

$script:OriginalSectionHeadersHex = @'
2e74657874000000b59e6e000010000000a06e0000040000000000000000000000000000200000602e72646174610000e9ec270000b06e0000ee270000a46e00000000000000000000000000400000402e64617461000000300c124400a0960000ea010000929600000000000000000000000000400000c02e706461746100009c06060000b0a84400080600007c9800000000000000000000000000400000402e69646174610000a68f000000c0ae440090000000849e00000000000000000000000000400000c02e64696461740000b60200000050af440004000000149f00000000000000000000000000400000c02e72737263000000b97500000060af440076000000189f0000000000000000000000000040000040
'@

$script:NewSectionHeaderHex = @'
2e6e7063666978004800000000e0af4400020000008e9f00000000000000000000
00000020000060
'@

$script:StubSectionHex = @'
4c8d470c8b17488b4c2470e9b0139abb4c8d470c8b17488b4c2470e9300f9abb
4c8d4f28440fb647088b5704488b4c2470e92a229abb4c8d4424288b5704488b4c
2470e9e8209abb
'@

function ConvertFrom-HexString {
    param([Parameter(Mandatory = $true)][string]$Hex)

    $clean = $Hex -replace '[^0-9A-Fa-f]', ''
    if (($clean.Length % 2) -ne 0) {
        throw 'Internal patch data contains an odd number of hexadecimal digits.'
    }

    $bytes = [byte[]]::new($clean.Length / 2)
    for ($index = 0; $index -lt $bytes.Length; $index++) {
        $bytes[$index] = [Convert]::ToByte($clean.Substring($index * 2, 2), 16)
    }
    return ,$bytes
}

function Get-Sha256Hex {
    param([Parameter(Mandatory = $true)][byte[]]$Bytes)

    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        return ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '')
    }
    finally {
        $sha.Dispose()
    }
}

function Read-UInt16LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    return [BitConverter]::ToUInt16($Bytes, $Offset)
}

function Read-UInt32LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    return [BitConverter]::ToUInt32($Bytes, $Offset)
}

function Read-UInt64LittleEndian {
    param([byte[]]$Bytes, [int]$Offset)
    return [BitConverter]::ToUInt64($Bytes, $Offset)
}

function Write-UInt16LittleEndian {
    param([byte[]]$Bytes, [int]$Offset, [UInt16]$Value)
    [BitConverter]::GetBytes($Value).CopyTo($Bytes, $Offset)
}

function Write-UInt32LittleEndian {
    param([byte[]]$Bytes, [int]$Offset, [UInt32]$Value)
    [BitConverter]::GetBytes($Value).CopyTo($Bytes, $Offset)
}

function Assert-ByteSequence {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Bytes,
        [Parameter(Mandatory = $true)][int]$Offset,
        [Parameter(Mandatory = $true)][byte[]]$Expected,
        [Parameter(Mandatory = $true)][string]$Description
    )

    if ($Offset -lt 0 -or ($Offset + $Expected.Length) -gt $Bytes.Length) {
        throw "$Description lies outside the input image."
    }
    for ($index = 0; $index -lt $Expected.Length; $index++) {
        if ($Bytes[$Offset + $index] -ne $Expected[$index]) {
            throw ("{0} differs at file offset 0x{1:X}." -f
                $Description, ($Offset + $index))
        }
    }
}

function Assert-ZeroRange {
    param([byte[]]$Bytes, [int]$Offset, [int]$Count, [string]$Description)

    for ($index = 0; $index -lt $Count; $index++) {
        if ($Bytes[$Offset + $index] -ne 0) {
            throw ("{0} is not empty at file offset 0x{1:X}." -f
                $Description, ($Offset + $index))
        }
    }
}

function Assert-OriginalZoneServerLayout {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Bytes,
        [switch]$SkipIdentityHashForTest
    )

    if ($Bytes.Length -ne $script:OriginalLength) {
        throw ("Unsupported ZoneServer size: {0} bytes; expected {1}." -f
            $Bytes.Length, $script:OriginalLength)
    }

    if (-not $SkipIdentityHashForTest) {
        $hash = Get-Sha256Hex $Bytes
        if ($hash -ne $script:OriginalSha256) {
            throw "Unsupported ZoneServer SHA-256: $hash"
        }
    }

    if ((Read-UInt16LittleEndian $Bytes 0) -ne 0x5A4D) {
        throw 'Input is not an MZ executable.'
    }
    $peOffset = [int](Read-UInt32LittleEndian $Bytes 0x3C)
    if ($peOffset -ne 0x118 -or
        (Read-UInt32LittleEndian $Bytes $peOffset) -ne 0x00004550) {
        throw 'Unexpected PE header location or signature.'
    }

    $fileHeader = $peOffset + 4
    $optionalHeader = $fileHeader + 20
    if ((Read-UInt16LittleEndian $Bytes $fileHeader) -ne 0x8664 -or
        (Read-UInt16LittleEndian $Bytes ($fileHeader + 2)) -ne 7 -or
        (Read-UInt32LittleEndian $Bytes ($fileHeader + 4)) -ne 0x4A7BAF5B -or
        (Read-UInt16LittleEndian $Bytes ($fileHeader + 16)) -ne 0xF0) {
        throw 'Unexpected x64 file header.'
    }

    if ((Read-UInt16LittleEndian $Bytes $optionalHeader) -ne 0x20B -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 4)) -ne 0x6EA000 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 16)) -ne 0x4DD320 -or
        (Read-UInt64LittleEndian $Bytes ($optionalHeader + 24)) -ne 0x140000000 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 32)) -ne 0x1000 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 36)) -ne 0x200 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 56)) -ne 0x44AFE000 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 60)) -ne 0x400 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 64)) -ne 0 -or
        (Read-UInt16LittleEndian $Bytes ($optionalHeader + 70)) -ne 0x8000 -or
        (Read-UInt32LittleEndian $Bytes ($optionalHeader + 108)) -ne 16) {
        throw 'Unexpected PE32+ optional header.'
    }

    $dataDirectories = $optionalHeader + 112
    if ((Read-UInt32LittleEndian $Bytes ($dataDirectories + 8)) -ne 0x44AEC000 -or
        (Read-UInt32LittleEndian $Bytes ($dataDirectories + 12)) -ne 0x1B8 -or
        (Read-UInt32LittleEndian $Bytes ($dataDirectories + 24)) -ne 0x44A8B000 -or
        (Read-UInt32LittleEndian $Bytes ($dataDirectories + 28)) -ne 0x57954 -or
        (Read-UInt32LittleEndian $Bytes ($dataDirectories + 40)) -ne 0 -or
        (Read-UInt32LittleEndian $Bytes ($dataDirectories + 44)) -ne 0) {
        throw 'Unexpected import, exception, or relocation directory.'
    }

    Assert-ByteSequence $Bytes 0x220 `
        (ConvertFrom-HexString $script:OriginalSectionHeadersHex) `
        'Original section table'
    Assert-ZeroRange $Bytes 0x338 (0x400 - 0x338) 'PE header slack'
    Assert-ByteSequence $Bytes $script:TargetOffset `
        (ConvertFrom-HexString $script:OriginalTargetHex) `
        'CCheckSumCharacAccountTrunkData::Update'
    Assert-ByteSequence $Bytes $script:UnwindOffset `
        (ConvertFrom-HexString $script:OriginalUnwindHex) `
        'CCheckSumCharacAccountTrunkData::Update unwind record'
    Assert-ByteSequence $Bytes $script:RuntimeFunctionOffset `
        (ConvertFrom-HexString $script:RuntimeFunctionHex) `
        'CCheckSumCharacAccountTrunkData::Update runtime-function record'
}

function Assert-UnchangedOutsidePatchRanges {
    param([byte[]]$Original, [byte[]]$Patched)

    $allowedRanges = @(
        [PSCustomObject]@{ Offset = 0x00011E; Count = 2 },
        [PSCustomObject]@{ Offset = 0x000134; Count = 4 },
        [PSCustomObject]@{ Offset = 0x000168; Count = 4 },
        [PSCustomObject]@{ Offset = 0x000338; Count = 0x28 },
        [PSCustomObject]@{
            Offset = $script:TargetOffset
            Count = $script:TargetLength
        },
        [PSCustomObject]@{ Offset = $script:UnwindOffset; Count = 8 }
    )

    $normalizedOriginal = [byte[]]$Original.Clone()
    $normalizedPatched = [byte[]]::new($script:OriginalLength)
    [Array]::Copy($Patched, 0, $normalizedPatched, 0, $script:OriginalLength)

    foreach ($range in $allowedRanges) {
        [Array]::Clear($normalizedOriginal, $range.Offset, $range.Count)
        [Array]::Clear($normalizedPatched, $range.Offset, $range.Count)
    }

    if ((Get-Sha256Hex $normalizedOriginal) -ne
        (Get-Sha256Hex $normalizedPatched)) {
        throw 'Patched image modifies bytes outside the audited offset allowlist.'
    }
}

function Get-RelativeBranchTargetRva {
    param([byte[]]$Bytes, [int]$FileOffset, [UInt32]$InstructionRva)

    $displacement = [BitConverter]::ToInt32($Bytes, $FileOffset + 1)
    return [UInt32]([Int64]$InstructionRva + 5 + $displacement)
}

function Assert-PatchedZoneServerLayout {
    param(
        [Parameter(Mandatory = $true)][byte[]]$Original,
        [Parameter(Mandatory = $true)][byte[]]$Patched,
        [switch]$SkipIdentityHashForTest
    )

    if ($Patched.Length -ne $script:PatchedLength) {
        throw 'Internal error: patched image has the wrong size.'
    }

    if (-not $SkipIdentityHashForTest) {
        $hash = Get-Sha256Hex $Patched
        if ($hash -ne $script:PatchedSha256) {
            throw "Internal error: patched image SHA-256 is $hash"
        }
    }

    if ((Read-UInt16LittleEndian $Patched 0x11E) -ne 8 -or
        (Read-UInt32LittleEndian $Patched 0x134) -ne 0x6EA200 -or
        (Read-UInt32LittleEndian $Patched 0x168) -ne 0x44AFF000) {
        throw 'Internal error: patched PE size fields are invalid.'
    }

    $patchedTarget = ConvertFrom-HexString $script:PatchedTargetHex
    Assert-ByteSequence $Patched $script:TargetOffset $patchedTarget `
        'Patched CCheckSumCharacAccountTrunkData::Update'
    Assert-ByteSequence $Patched $script:UnwindOffset `
        (ConvertFrom-HexString $script:PatchedUnwindHex) `
        'Patched unwind record'
    Assert-ByteSequence $Patched 0x338 `
        (ConvertFrom-HexString $script:NewSectionHeaderHex) `
        'New .npcfix section header'
    Assert-ByteSequence $Patched $script:NewSectionOffset `
        (ConvertFrom-HexString $script:StubSectionHex) `
        'New .npcfix tail-call stubs'

    Assert-ZeroRange $Patched `
        ($script:NewSectionOffset + 0x48) `
        (0x200 - 0x48) `
        '.npcfix raw padding'

    $paddingOffset = $script:TargetOffset + $patchedTarget.Length
    for ($offset = $paddingOffset;
         $offset -lt ($script:TargetOffset + $script:TargetLength);
         $offset++) {
        if ($Patched[$offset] -ne 0xCC) {
            throw 'Internal error: old function tail was not filled with INT3.'
        }
    }

    Assert-UnchangedOutsidePatchRanges $Original $Patched

    $callOffsets = @(0x17, 0x20, 0x29, 0x32, 0x67, 0x70)
    $callTargets = @(
        0x44AFE000,
        0x44AFE010,
        0x44AFE000,
        0x44AFE020,
        0x44AFE036,
        0x44AFE020
    )
    for ($index = 0; $index -lt $callOffsets.Count; $index++) {
        $instructionOffset = $script:TargetOffset + $callOffsets[$index]
        if ($Patched[$instructionOffset] -ne 0xE8) {
            throw 'Internal error: expected relative CALL opcode is missing.'
        }
        $target = Get-RelativeBranchTargetRva $Patched $instructionOffset `
            ([UInt32](0x2C0B60 + $callOffsets[$index]))
        if ($target -ne $callTargets[$index]) {
            throw ("Internal error: CALL target 0x{0:X} is invalid." -f $target)
        }
    }

    $jumpOffsets = @(0x0B, 0x1B, 0x31, 0x43)
    $jumpTargets = @(0x49F3C0, 0x49EF50, 0x4A0260, 0x4A0130)
    for ($index = 0; $index -lt $jumpOffsets.Count; $index++) {
        $instructionOffset = $script:NewSectionOffset + $jumpOffsets[$index]
        if ($Patched[$instructionOffset] -ne 0xE9) {
            throw 'Internal error: expected relative JMP opcode is missing.'
        }
        $target = Get-RelativeBranchTargetRva $Patched $instructionOffset `
            ([UInt32](0x44AFE000 + $jumpOffsets[$index]))
        if ($target -ne $jumpTargets[$index]) {
            throw ("Internal error: DB tail-call target 0x{0:X} is invalid." -f $target)
        }
    }
}

function New-NpcDataPatchedImage {
    param(
        [Parameter(Mandatory = $true)][byte[]]$InputBytes,
        [switch]$TestFixture
    )

    Assert-OriginalZoneServerLayout $InputBytes `
        -SkipIdentityHashForTest:$TestFixture

    $patched = [byte[]]::new($script:PatchedLength)
    [Array]::Copy($InputBytes, 0, $patched, 0, $InputBytes.Length)

    Write-UInt16LittleEndian $patched 0x11E 8
    Write-UInt32LittleEndian $patched 0x134 0x6EA200
    Write-UInt32LittleEndian $patched 0x168 0x44AFF000

    (ConvertFrom-HexString $script:NewSectionHeaderHex).CopyTo($patched, 0x338)

    $patchedTarget = ConvertFrom-HexString $script:PatchedTargetHex
    $patchedTarget.CopyTo($patched, $script:TargetOffset)
    for ($offset = $script:TargetOffset + $patchedTarget.Length;
         $offset -lt ($script:TargetOffset + $script:TargetLength);
         $offset++) {
        $patched[$offset] = 0xCC
    }

    (ConvertFrom-HexString $script:PatchedUnwindHex).CopyTo(
        $patched, $script:UnwindOffset)
    (ConvertFrom-HexString $script:StubSectionHex).CopyTo(
        $patched, $script:NewSectionOffset)

    Assert-PatchedZoneServerLayout $InputBytes $patched `
        -SkipIdentityHashForTest:$TestFixture
    return ,$patched
}

function Invoke-ZoneServerInlinePatch {
    param(
        [Parameter(Mandatory = $true)][string]$SourcePath,
        [string]$DestinationPath
    )

    $source = [IO.Path]::GetFullPath($SourcePath)
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Input file does not exist: $source"
    }

    if ([string]::IsNullOrWhiteSpace($DestinationPath)) {
        $directory = [IO.Path]::GetDirectoryName($source)
        $name = [IO.Path]::GetFileNameWithoutExtension($source)
        $destination = Join-Path $directory ($name + '.NpcDataFix.exe')
    }
    else {
        $destination = [IO.Path]::GetFullPath($DestinationPath)
    }

    if ([string]::Equals($source, $destination,
            [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Refusing to patch the input file in place.'
    }
    if (Test-Path -LiteralPath $destination) {
        throw "Output already exists: $destination"
    }

    $destinationDirectory = [IO.Path]::GetDirectoryName($destination)
    if (-not (Test-Path -LiteralPath $destinationDirectory -PathType Container)) {
        throw "Output directory does not exist: $destinationDirectory"
    }

    $inputBytes = [IO.File]::ReadAllBytes($source)
    $patchedBytes = New-NpcDataPatchedImage $inputBytes

    $temporary = Join-Path $destinationDirectory (
        '.' + [IO.Path]::GetFileName($destination) + '.' +
        [Guid]::NewGuid().ToString('N') + '.tmp')
    $completed = $false
    try {
        $stream = [IO.File]::Open(
            $temporary,
            [IO.FileMode]::CreateNew,
            [IO.FileAccess]::Write,
            [IO.FileShare]::None)
        try {
            $stream.Write($patchedBytes, 0, $patchedBytes.Length)
            $stream.Flush($true)
        }
        finally {
            $stream.Dispose()
        }

        $writtenHash = (Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash
        if ($writtenHash -ne $script:PatchedSha256) {
            throw "Written output SHA-256 is invalid: $writtenHash"
        }

        [IO.File]::Move($temporary, $destination)
        $completed = $true
    }
    finally {
        if (-not $completed -and (Test-Path -LiteralPath $temporary -PathType Leaf)) {
            Remove-Item -LiteralPath $temporary
        }
    }

    Write-Host 'DLL-free NpcData patch created successfully.'
    Write-Host "Input:   $source"
    Write-Host "Output:  $destination"
    Write-Host "SHA-256: $($script:PatchedSha256)"
    Write-Host 'The original file was not modified.'
    return $destination
}

if (-not $LibraryMode) {
    if ([string]::IsNullOrWhiteSpace($InputPath)) {
        throw 'Specify the original ZoneServerUD_x64.exe as InputPath.'
    }
    Invoke-ZoneServerInlinePatch $InputPath $OutputPath | Out-Null
}
