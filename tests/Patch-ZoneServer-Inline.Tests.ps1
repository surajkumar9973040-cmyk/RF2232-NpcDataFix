$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$patcher = Join-Path $PSScriptRoot '..\Patch-ZoneServer-Inline.ps1'
. $patcher -LibraryMode

function Assert-True {
    param([bool]$Condition, [string]$Message)
    if (-not $Condition) {
        throw $Message
    }
}

function Assert-Throws {
    param([scriptblock]$Action, [string]$Message)

    $thrown = $false
    try {
        & $Action
    }
    catch {
        $thrown = $true
    }
    if (-not $thrown) {
        throw $Message
    }
}

function New-SyntheticOriginalImage {
    $bytes = [byte[]]::new($script:OriginalLength)

    Write-UInt16LittleEndian $bytes 0x000 0x5A4D
    Write-UInt32LittleEndian $bytes 0x03C 0x118
    Write-UInt32LittleEndian $bytes 0x118 0x00004550

    Write-UInt16LittleEndian $bytes 0x11C 0x8664
    Write-UInt16LittleEndian $bytes 0x11E 7
    Write-UInt32LittleEndian $bytes 0x120 0x4A7BAF5B
    Write-UInt16LittleEndian $bytes 0x12C 0xF0

    Write-UInt16LittleEndian $bytes 0x130 0x20B
    Write-UInt32LittleEndian $bytes 0x134 0x6EA000
    Write-UInt32LittleEndian $bytes 0x140 0x4DD320
    [BitConverter]::GetBytes([UInt64]0x140000000).CopyTo($bytes, 0x148)
    Write-UInt32LittleEndian $bytes 0x150 0x1000
    Write-UInt32LittleEndian $bytes 0x154 0x200
    Write-UInt32LittleEndian $bytes 0x168 0x44AFE000
    Write-UInt32LittleEndian $bytes 0x16C 0x400
    Write-UInt32LittleEndian $bytes 0x170 0
    Write-UInt16LittleEndian $bytes 0x176 0x8000
    Write-UInt32LittleEndian $bytes 0x19C 16

    Write-UInt32LittleEndian $bytes 0x1A8 0x44AEC000
    Write-UInt32LittleEndian $bytes 0x1AC 0x1B8
    Write-UInt32LittleEndian $bytes 0x1B8 0x44A8B000
    Write-UInt32LittleEndian $bytes 0x1BC 0x57954

    (ConvertFrom-HexString $script:OriginalSectionHeadersHex).CopyTo($bytes, 0x220)
    (ConvertFrom-HexString $script:OriginalTargetHex).CopyTo(
        $bytes, $script:TargetOffset)
    (ConvertFrom-HexString $script:OriginalUnwindHex).CopyTo(
        $bytes, $script:UnwindOffset)
    (ConvertFrom-HexString $script:RuntimeFunctionHex).CopyTo(
        $bytes, $script:RuntimeFunctionOffset)

    return ,$bytes
}

$original = New-SyntheticOriginalImage
$originalHashBefore = Get-Sha256Hex $original
$patched = New-NpcDataPatchedImage $original -TestFixture
$originalHashAfter = Get-Sha256Hex $original

Assert-True ($originalHashBefore -eq $originalHashAfter) `
    'The patcher modified its input buffer.'
Assert-True ($patched.Length -eq $script:PatchedLength) `
    'The synthetic patched image has the wrong size.'
Assert-True ((Read-UInt16LittleEndian $patched 0x11E) -eq 8) `
    'NumberOfSections was not updated.'
Assert-True ((Read-UInt32LittleEndian $patched 0x134) -eq 0x6EA200) `
    'SizeOfCode was not updated.'
Assert-True ((Read-UInt32LittleEndian $patched 0x168) -eq 0x44AFF000) `
    'SizeOfImage was not updated.'

$secondOriginal = New-SyntheticOriginalImage
$secondPatched = New-NpcDataPatchedImage $secondOriginal -TestFixture
Assert-True ((Get-Sha256Hex $patched) -eq (Get-Sha256Hex $secondPatched)) `
    'The inline transformation is not deterministic.'

$wrongTarget = New-SyntheticOriginalImage
$wrongTarget[$script:TargetOffset] = $wrongTarget[$script:TargetOffset] -bxor 0x01
Assert-Throws { New-NpcDataPatchedImage $wrongTarget -TestFixture } `
    'A modified target function was accepted.'

$wrongUnwind = New-SyntheticOriginalImage
$wrongUnwind[$script:UnwindOffset] = $wrongUnwind[$script:UnwindOffset] -bxor 0x01
Assert-Throws { New-NpcDataPatchedImage $wrongUnwind -TestFixture } `
    'A modified unwind record was accepted.'

$wrongRuntimeFunction = New-SyntheticOriginalImage
$wrongRuntimeFunction[$script:RuntimeFunctionOffset] =
    $wrongRuntimeFunction[$script:RuntimeFunctionOffset] -bxor 0x01
Assert-Throws { New-NpcDataPatchedImage $wrongRuntimeFunction -TestFixture } `
    'A modified runtime-function record was accepted.'

$occupiedHeader = New-SyntheticOriginalImage
$occupiedHeader[0x360] = 0x41
Assert-Throws { New-NpcDataPatchedImage $occupiedHeader -TestFixture } `
    'Occupied PE header slack was accepted.'

$wrongHashFixture = New-SyntheticOriginalImage
Assert-Throws { Assert-OriginalZoneServerLayout $wrongHashFixture } `
    'Production identity validation accepted a synthetic file.'

Write-Host 'Inline ZoneServer patcher tests passed.'
