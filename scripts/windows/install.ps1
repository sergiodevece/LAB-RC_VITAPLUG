<#
.SYNOPSIS
Instala, desinstala o restaura el VST3 LAB-RC para Windows x64.
.DESCRIPTION
Cierra el DAW y abre PowerShell x64 como administrador. El destino es
Common Files\VST3. Las copias anteriores quedan en Common Files\VitaPlug LAB-RC Backups.
.EXAMPLE
.\install.ps1 -Action Install
.EXAMPLE
.\install.ps1 -Action Uninstall
.EXAMPLE
.\install.ps1 -Action Restore -BackupPath 'C:\...\LAB-RC.vst3'
.EXAMPLE
.\install.ps1 -WhatIf
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [ValidateSet('Install', 'Uninstall', 'Restore')]
    [string]$Action = 'Install',

    [string]$BackupPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$bundleName = 'LAB-RC.vst3'

function Assert-PlainDirectory([string]$Path) {
    $directory = Get-Item -LiteralPath $Path -Force
    if (-not $directory.PSIsContainer -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Se esperaba una carpeta local sin redirecciones en $Path"
    }
}

function Get-LabRCFingerprint([string]$Bundle) {
    Assert-PlainDirectory $Bundle
    $reparsePoint = Get-ChildItem -LiteralPath $Bundle -Force -Recurse |
        Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint } |
        Select-Object -First 1
    if ($null -ne $reparsePoint) {
        throw "El bundle contiene una redireccion en $($reparsePoint.FullName)"
    }

    $manifestPath = Join-Path $Bundle 'Contents\Resources\moduleinfo.json'
    $raw = Get-Content -LiteralPath $manifestPath -Raw -Encoding UTF8
    # JUCE writes a JSON5 manifest with trailing commas.
    $manifest = [regex]::Replace($raw, '("(?:\\.|[^"\\])*")|,(?=\s*[}\]])', '$1') | ConvertFrom-Json
    if ($manifest.Name -cne 'LAB-RC' -or $manifest.'Factory Info'.Vendor -cne 'VITAPLUG') {
        throw "La identidad del bundle no corresponde a LAB-RC de VITAPLUG en $Bundle"
    }

    $binary = Join-Path $Bundle 'Contents\x86_64-win\LAB-RC.vst3'
    $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($binary))
    try {
        if ($reader.BaseStream.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) {
            throw "El binario no es un PE de Windows en $binary"
        }
        $reader.BaseStream.Position = 0x3C
        $offset = $reader.ReadUInt32()
        if ($offset -lt 64 -or ([long]$offset + 26) -gt $reader.BaseStream.Length) {
            throw "La cabecera PE esta incompleta en $binary"
        }
        $reader.BaseStream.Position = $offset
        if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
            throw "El binario no es AMD64 en $binary"
        }
        $reader.BaseStream.Position = $offset + 22
        if (-not ($reader.ReadUInt16() -band 0x2000) -or $reader.ReadUInt16() -ne 0x20B) {
            throw "El binario no es una DLL PE32+ en $binary"
        }
        # Keep the read-only hash independent of the caller's WhatIf preference.
        $sha256 = [Security.Cryptography.SHA256]::Create()
        try {
            $reader.BaseStream.Position = 0
            $hash = [BitConverter]::ToString($sha256.ComputeHash($reader.BaseStream)).Replace('-', '')
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $reader.Dispose()
    }
    return [pscustomobject]@{
        Hash = $hash
        Version = $manifest.Version
    }
}

if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'Este instalador requiere Windows x64.'
}
$osArchitecture = $env:PROCESSOR_ARCHITECTURE
if ('System.Runtime.InteropServices.RuntimeInformation' -as [type]) {
    $osArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture.ToString()
}
if (-not [Environment]::Is64BitProcess -or $osArchitecture -notin @('X64', 'AMD64')) {
    throw 'Abre una consola de PowerShell x64 en Windows x64.'
}

$common = [Environment]::GetFolderPath([Environment+SpecialFolder]::CommonProgramFiles)
Assert-PlainDirectory $common
$pluginRoot = Join-Path $common 'VST3'
$target = Join-Path $pluginRoot $bundleName
$backups = Join-Path $common 'VitaPlug LAB-RC Backups'

if ($Action -ne 'Restore' -and $BackupPath) {
    throw 'BackupPath se utiliza con Action Restore.'
}

$source = $null
$incoming = $null
if ($Action -eq 'Install') {
    $source = Join-Path $PSScriptRoot $bundleName
    $receiptPath = Join-Path $PSScriptRoot 'build-info.json'
    $receipt = Get-Content -LiteralPath $receiptPath -Raw -Encoding UTF8 | ConvertFrom-Json
    $incoming = Get-LabRCFingerprint $source
    if ($receipt.architecture -cne 'Windows x64' -or $receipt.configuration -cne 'Release' -or
        $receipt.version -cne $incoming.Version -or $receipt.binary_sha256 -ne $incoming.Hash) {
        throw 'La identidad, la version o el SHA-256 no coinciden con build-info.json.'
    }
} elseif ($Action -eq 'Restore') {
    if (-not $BackupPath) {
        throw 'Indica la ruta LAB-RC.vst3 del respaldo con BackupPath.'
    }
    Assert-PlainDirectory $backups
    $source = (Get-Item -LiteralPath $BackupPath -Force).FullName
    $backupFolder = [IO.Path]::GetDirectoryName($source)
    if ([IO.Path]::GetFileName($source) -ne $bundleName -or
        [IO.Path]::GetDirectoryName($backupFolder) -ne $backups) {
        throw "Selecciona una copia dentro de $backups"
    }
    Assert-PlainDirectory $backupFolder
    $incoming = Get-LabRCFingerprint $source
    $savedHash = (Get-Content -LiteralPath (Join-Path $backupFolder 'SHA256.txt') -Raw).Trim()
    if ($savedHash -ne $incoming.Hash) {
        throw 'La copia de respaldo no coincide con su SHA-256 guardado.'
    }
}

if (Test-Path -LiteralPath $pluginRoot) { Assert-PlainDirectory $pluginRoot }
if (-not (Test-Path -LiteralPath $target) -and $Action -eq 'Uninstall') {
    Write-Output "LAB-RC no esta instalado en $target"
    return
}
if (-not $PSCmdlet.ShouldProcess($target, "$Action LAB-RC")) {
    return
}
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Ejecuta PowerShell como administrador y cierra el DAW antes de continuar.'
}

[void][IO.Directory]::CreateDirectory($backups)
Assert-PlainDirectory $backups
$lock = [IO.File]::Open((Join-Path $backups 'install.lock'),
    [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::Write, [IO.FileShare]::None)
$work = $null
$stage = $null
$archived = $null
try {
    $current = $null
    if (Test-Path -LiteralPath $target) { $current = Get-LabRCFingerprint $target }
    if ($null -eq $current -and $Action -eq 'Uninstall') {
        Write-Output "LAB-RC no esta instalado en $target"
        return
    }
    if ($null -ne $current -and $null -ne $incoming -and $current.Hash -eq $incoming.Hash) {
        Write-Output "LAB-RC $($incoming.Version) ya esta instalado en $target"
        return
    }

    $work = Join-Path $backups ('{0:yyyyMMdd-HHmmss}-{1}' -f (Get-Date), [guid]::NewGuid().ToString('N'))
    [void][IO.Directory]::CreateDirectory($work)
    $stage = Join-Path $work 'staged.vst3'
    $archived = Join-Path $work $bundleName
    if ($null -ne $source) {
        Copy-Item -LiteralPath $source -Destination $stage -Recurse
        if ((Get-LabRCFingerprint $stage).Hash -ne $incoming.Hash) {
            throw 'La copia preparada no coincide con el SHA-256 comprobado.'
        }
        [void][IO.Directory]::CreateDirectory($pluginRoot)
        Assert-PlainDirectory $pluginRoot
    }

    if ($null -ne $current) {
        Set-Content -LiteralPath (Join-Path $work 'SHA256.txt') -Value $current.Hash -Encoding ASCII
        [System.IO.Directory]::Move($target, $archived)
    }

    if ($null -eq $source) {
        Write-Output "Copia anterior $archived"
        Write-Output "LAB-RC desinstalado de $target"
        return
    }
    try {
        [System.IO.Directory]::Move($stage, $target)
    } catch {
        if ([IO.Directory]::Exists($archived)) {
            [System.IO.Directory]::Move($archived, $target)
        }
        throw
    }
    if ($null -ne $current) {
        Write-Output "Copia anterior $archived"
    }
    Write-Output "LAB-RC $($incoming.Version) instalado en $target"
} finally {
    if ([IO.Directory]::Exists($work)) {
        if ([IO.Directory]::Exists($stage)) {
            Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
        }
        if (-not [IO.Directory]::Exists($archived)) {
            Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
    $lock.Dispose()
}
