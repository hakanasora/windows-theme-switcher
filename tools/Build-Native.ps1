param(
    [switch]$Installer,
    [switch]$Release,
    [switch]$DevelopmentCertificate,
    [switch]$UnsignedPrototype,
    [string]$CertificateThumbprint,
    [string]$Publisher,
    [string]$IsccPath,
    [switch]$SkipTests,
    [switch]$Clean
)
$ErrorActionPreference = 'Stop'
if ($Release -and (-not $Installer -or $DevelopmentCertificate -or $SkipTests)) {
    throw 'Release packaging requires -Installer, enabled tests, and no development certificate.'
}
$root = Split-Path $PSScriptRoot -Parent
$buildRoot = Join-Path $root 'build\native'
$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw 'VERSION must contain major.minor.patch.' }
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio Build Tools with MSVC, CMake and Windows SDK.' }
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'MSVC x64 build tools were not found.' }
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
# Import the vendor's process-local build environment. No persistent PATH edits.
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$environmentScript = Join-Path $buildRoot 'load-vc-environment.cmd'
[IO.File]::WriteAllText($environmentScript,"@call `"$vcvars`" >nul`r`n@if errorlevel 1 exit /b 1`r`n@set`r`n",[Text.Encoding]::Default)
$environmentLines = & $env:ComSpec /d /c $environmentScript
if ($LASTEXITCODE -ne 0) { throw 'Unable to initialize the MSVC environment.' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=][^=]*)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1],$Matches[2],'Process') }
}
$env:VSLANG = '1033'
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
& $cmake -S $root -B $buildRoot -G 'Ninja' -DCMAKE_BUILD_TYPE=Release '-DCMAKE_CL_SHOWINCLUDES_PREFIX=Note: including file:'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
if ($Clean) {
    & $cmake --build $buildRoot --target clean
    if ($LASTEXITCODE -ne 0) { throw 'Build artifact cleanup failed.' }
}
& $cmake --build $buildRoot --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
if (-not $SkipTests) {
    $ctest = Join-Path (Split-Path $cmake -Parent) 'ctest.exe'
    & $ctest --test-dir $buildRoot --output-on-failure -V
    if ($LASTEXITCODE -ne 0) { throw 'Native verification failed.' }
}
$artifacts = Join-Path $buildRoot 'artifacts'
Write-Host "Native artifacts: $artifacts"
if (-not $Installer) { return }

$sdkBin = Get-ChildItem -LiteralPath "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Directory |
    Where-Object { $_.Name -match '^10\.0\.' -and (Test-Path -LiteralPath (Join-Path $_.FullName 'x64\makeappx.exe')) } |
    Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
if (-not $sdkBin) { throw 'Windows SDK MakeAppx and SignTool were not found.' }
$makeappx = Join-Path $sdkBin.FullName 'x64\makeappx.exe'
$signtool = Join-Path $sdkBin.FullName 'x64\signtool.exe'
New-Item -ItemType Directory -Path (Join-Path $root 'dist\v2-development') -Force | Out-Null
$dist = Join-Path $root $(if ($Release) { 'dist\release' } elseif ($UnsignedPrototype) { 'dist\v2-unsigned-prototype' } else { 'dist\v2-development' })
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$packageName = 'HakanaSora.ThemeSwitcher'
if ($UnsignedPrototype) {
    if ($DevelopmentCertificate -or $CertificateThumbprint -or $Publisher) { throw 'Unsigned prototype cannot be combined with signing parameters.' }
    $packageName += '.UnsignedDevelopment'
    $Publisher = 'CN=Hakana Sora ThemeSwitcher Unsigned Development, OID.2.25.311729368913984317654407730594956997722=1'
} elseif ($DevelopmentCertificate) {
    if ($CertificateThumbprint) { throw 'Choose either a development certificate or an existing signing certificate.' }
    $packageName += '.Development'
    $Publisher = 'CN=Hakana Sora ThemeSwitcher Development'
    $record = Join-Path $buildRoot 'development-certificate-thumbprint.txt'
    $certificate = $null
    if (Test-Path -LiteralPath $record) {
        $savedThumbprint = (Get-Content -LiteralPath $record -Raw).Trim()
        $certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$savedThumbprint" -ErrorAction SilentlyContinue
    }
    if (-not $certificate -or -not $certificate.HasPrivateKey -or $certificate.NotAfter -le (Get-Date)) {
        $certificate = New-SelfSignedCertificate -Type Custom -Subject $Publisher -CertStoreLocation 'Cert:\CurrentUser\My' `
            -KeyAlgorithm RSA -KeyLength 3072 -KeyUsage DigitalSignature -KeyExportPolicy NonExportable `
            -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3') -NotAfter (Get-Date).AddYears(1)
        $certificate.Thumbprint | Set-Content -LiteralPath $record -Encoding ascii
    }
    $CertificateThumbprint = $certificate.Thumbprint
    Export-Certificate -Cert $certificate -FilePath (Join-Path $dist 'ThemeSwitcher.Development.cer') -Force | Out-Null
    @("Subject: $($certificate.Subject)","SHA-1 thumbprint: $($certificate.Thumbprint)","Expires: $($certificate.NotAfter.ToString('yyyy-MM-dd'))",'Public development certificate only. No trust store was modified.') |
        Set-Content -LiteralPath (Join-Path $dist 'DEVELOPMENT-CERTIFICATE.txt') -Encoding utf8
} elseif (-not $CertificateThumbprint -or -not $Publisher) {
    throw 'Installer packaging requires -DevelopmentCertificate or an existing -CertificateThumbprint and -Publisher.'
}
if (-not $UnsignedPrototype -and $CertificateThumbprint -notmatch '^[0-9a-fA-F]{40}$') { throw 'Invalid signing certificate thumbprint.' }
$stage = Join-Path $buildRoot 'identity-stage'
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$manifest = Get-Content -LiteralPath (Join-Path $root 'packaging\AppxManifest.xml.in') -Raw
$manifest = $manifest.Replace('@PACKAGE_NAME@',$packageName).Replace('@PUBLISHER@',[Security.SecurityElement]::Escape($Publisher)).Replace('@VERSION@',$version)
[IO.File]::WriteAllText((Join-Path $stage 'AppxManifest.xml'),$manifest,[Text.UTF8Encoding]::new($false))
$componentRoot = Join-Path $artifacts "components\$version"
$identity = Join-Path $componentRoot 'identity.msix'
& $makeappx pack /o /nv /d $stage /p $identity
if ($LASTEXITCODE -ne 0) { throw 'Identity package creation failed.' }
if (-not $UnsignedPrototype) {
    & $signtool sign /fd SHA256 /sha1 $CertificateThumbprint /s My $identity
    if ($LASTEXITCODE -ne 0) { throw 'Identity package signing failed.' }
}
Copy-Item -LiteralPath (Join-Path $artifacts 'ThemeSwitcher.Deploy.exe') -Destination $componentRoot
Copy-Item -LiteralPath (Join-Path $artifacts 'ThemeSwitcher.MenuSetup.exe') -Destination $componentRoot
'HakanaSora.ThemeSwitcher' | Set-Content -LiteralPath (Join-Path $componentRoot 'owner.txt') -Encoding ascii
if (-not $IsccPath) {
    foreach ($candidate in @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe","$env:ProgramFiles\Inno Setup 6\ISCC.exe","$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")) {
        if (Test-Path -LiteralPath $candidate) { $IsccPath=$candidate; break }
    }
}
if (-not $IsccPath) { throw 'Install Inno Setup 6 or supply -IsccPath.' }
$preprocessed = Join-Path $buildRoot 'installer-preprocessed.iss'
& $IsccPath "/DBuildRoot=$artifacts" "/DRepoRoot=$root" "/DAppVersion=$version" "/DDevelopmentBuild=$([int][bool](-not $Release))" "/DUnsignedPrototype=$([int][bool]$UnsignedPrototype)" "/DPreprocessedOutput=$preprocessed" "/O$dist" (Join-Path $root 'InstallerScript.iss')
if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed.' }
$expectedName = "Windows Theme Switcher $version" + $(if ($Release) { '' } else { ' 本机验证版' })
$actualName = [regex]::Match([IO.File]::ReadAllText($preprocessed),'(?m)^AppVerName=(.*)\r?$').Groups[1].Value.Trim()
if ($actualName -ne $expectedName) { throw "Incorrect installer name: $actualName" }
Write-Host "Verified installer name: $actualName"
Write-Host "Installer: $(Join-Path $dist 'ThemeSwitcherSetup.exe')"
if ($DevelopmentCertificate) { Write-Host 'Development package signed. No certificate trust or application registration was changed.' }
if ($UnsignedPrototype) { Write-Host 'Unsigned identity package. No certificates were created, exported or trusted.' }
