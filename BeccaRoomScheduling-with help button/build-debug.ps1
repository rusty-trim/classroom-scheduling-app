$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $projectRoot "build\Desktop_Qt_6_10_0_MinGW_64_bit-Debug"

$env:PATH = @(
    "C:\Qt\Tools\mingw1310_64\bin",
    "C:\Qt\6.10.0\mingw_64\bin",
    "C:\Windows\system32",
    "C:\Windows",
    "C:\Windows\System32\Wbem",
    "C:\Windows\System32\WindowsPowerShell\v1.0\"
) -join ";"

Push-Location $buildDir
try {
    & "C:\Qt\Tools\mingw1310_64\bin\mingw32-make.exe" -j12
}
finally {
    Pop-Location
}
