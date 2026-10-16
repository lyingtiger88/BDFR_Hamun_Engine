# SPDX-License-Identifier: GPL-2.0-or-later
<#
Inventory every distributable path containing "blender" without modifying files.
Outputs TSV with relative path, type and recommended handling.
Usage:
powershell -NoProfile -ExecutionPolicy Bypass -File .\integrations\blender\audit_branding.ps1
#>
[CmdletBinding()]
param(
  [string]$ReleaseDir = 'D:\HamunDev\build_windows_Release_x64_vc17_Release\bin\Release',
  [string]$ReportPath = 'D:\HamunDev\Hamun-branding-inventory.tsv'
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath $ReleaseDir).Path.TrimEnd('\')
$rows = @(
  Get-ChildItem -LiteralPath $root -Recurse -Force |
  Where-Object { $_.Name -match '(?i)blender' } |
  ForEach-Object {
    $rel = $_.FullName.Substring($root.Length).TrimStart('\')
    $name = $_.Name
    $kind = if ($_.PSIsContainer) { 'Directory' } else { 'File' }
    $action = if ($rel -match '(^|\\)license(\\|$)|(^|\\)licenses?(\\|$)') {
      'RETAIN: license/provenance'
    } elseif ($rel -match '^blender\.exe$') {
      'EXCLUDE if HamunEditorCore.exe is functional'
    } elseif ($rel -match '^blender-launcher\.exe$') {
      'EXCLUDE if HamunEditor.exe is functional'
    } elseif ($rel -match '\.(pdb|lib|exp|ilk)$') {
      'EXCLUDE: build/debug artifact'
    } elseif ($rel -match '^blender\.(crt|shared)(\\|$)|\.(dll|exe|pyd)$') {
      'REVIEW: binary/side-by-side dependency'
    } elseif ($rel -match '\.(cmd|bat)$') {
      'REVIEW: rename AND rewrite command contents'
    } else {
      'REVIEW: path references and runtime dependencies'
    }
    [pscustomobject]@{RelativePath=$rel;Type=$kind;Action=$action}
  }
)
$parent = Split-Path -Parent $ReportPath
if ($parent) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
$rows | Export-Csv -LiteralPath $ReportPath -Delimiter "`t" -NoTypeInformation -Encoding UTF8
$rows | Format-Table -AutoSize
Write-Host "Inventory: $ReportPath"
Write-Host "Matching paths: $($rows.Count)"
Write-Host 'No files were renamed, moved or deleted.'
