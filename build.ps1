param(
    [switch]$Installer,
    [string]$IsccPath
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$compilerPaths = @(
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'),
    (Join-Path $env:WINDIR 'Microsoft.NET\Framework\v4.0.30319\csc.exe')
)
$compiler = $compilerPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $compiler) {
    throw 'The .NET Framework 4 C# compiler was not found.'
}

$exePath = Join-Path $root 'ThemeSwitcher.exe'
& $compiler /nologo /target:winexe /platform:anycpu /optimize- /debug- /codepage:65001 `
    "/out:$exePath" "/win32icon:$(Join-Path $root 'MyIcon.ico')" `
    (Join-Path $root 'src\ThemeSwitcher.cs')
if ($LASTEXITCODE -ne 0) { throw "C# compilation failed with exit code $LASTEXITCODE." }
Write-Host "Built: $exePath"

if ($Installer) {
    if (-not $IsccPath) {
        $isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($isccCommand) { $IsccPath = $isccCommand.Source }
    }
    if (-not $IsccPath) {
        $innoPaths = @(
            "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
            "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
        )
        $IsccPath = $innoPaths | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $IsccPath -or -not (Test-Path -LiteralPath $IsccPath)) {
        throw 'Inno Setup was not found. Supply -IsccPath with the path to ISCC.exe.'
    }

    $distPath = Join-Path $root 'dist'
    New-Item -ItemType Directory -Path $distPath -Force | Out-Null
    & $IsccPath "/O$distPath" (Join-Path $root 'InstallerScript.iss')
    if ($LASTEXITCODE -ne 0) { throw "Installer compilation failed with exit code $LASTEXITCODE." }
    Write-Host "Built: $(Join-Path $distPath 'ThemeSwitcherSetup.exe')"
}
