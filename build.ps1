param([switch]$Installer,[switch]$DevelopmentCertificate,[string]$CertificateThumbprint,[string]$Publisher,[string]$IsccPath,[switch]$SkipTests,[switch]$Clean)
& (Join-Path $PSScriptRoot 'tools\Build-Native.ps1') @PSBoundParameters
