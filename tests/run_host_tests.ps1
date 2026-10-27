param([switch]$AdcOnly, [switch]$CanOnly, [switch]$AnnOnly, [switch]$AutoTestOnly, [switch]$PllOnly, [switch]$VirtualAngleOnly, [string]$AnnImplementationOverride = '')
$ErrorActionPreference = 'Stop'
$projectPath = Split-Path -Parent $PSScriptRoot
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstallPath = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vsInstallPath) { throw 'MSVC C compiler is required for host tests.' }
$vcvarsPath = Join-Path $vsInstallPath 'VC\Auxiliary\Build\vcvars64.bat'
$buildCheckPath = Join-Path ([System.IO.Path]::GetTempPath()) ('vsg-inverter-host-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $buildCheckPath | Out-Null
$sources = @('tests/inverter_host_test.c', 'app/inverter.c', 'app/inverter_settings.c', 'app/measurement/adc_measurement.c', 'app/control/inner_loop.c', 'app/control/presync.c', 'app/control/vsg.c', 'app/control/inverter_state.c', 'app/math/pi_controller.c', 'app/measurement/feedback_dq.c', 'app/control/three_level_modulation.c', 'app/hardware/pwm_output.c', 'app/PLL/grid_pll.c', 'app/PLL/ABC_DQ0_POS_F.c', 'app/PLL/ABC_DQ0_NEG_F.c', 'app/PLL/SPLL_3ph_DDSRF_F.c')
$sources += @('tests/ann_migration_test.c', 'app/adaptive/ann_models.c', 'app/adaptive/ann_adaptive.c')
$sources += @('tests/can_migration_test.c', 'app/communication/can_control.c')
$sources += @('tests/vsg_auto_test_test.c', 'app/control/vsg_auto_test.c')
$sources += @('tests/pll_optimization_test.c')
$sources += @('tests/virtual_angle_test.c')
$sources += @('app/control/reactive_control.c', 'tests/reactive_control_test.c')
$quotedSources = ($sources | ForEach-Object { $sourcePath = if($_ -eq 'app/adaptive/ann_adaptive.c' -and $AnnImplementationOverride) { $AnnImplementationOverride } else { Join-Path $projectPath $_ }; '"' + $sourcePath + '"' }) -join ' '
$buildCommand = 'call "' + $vcvarsPath + '" >nul && cd /d "' + $buildCheckPath + '" && cl.exe /nologo /std:c17 /utf-8 /W3 /Od /I"' + (Join-Path $PSScriptRoot 'stubs') + '" /I"' + $projectPath + '" ' + $quotedSources + ' /I"' + (Join-Path $projectPath 'app/adaptive') + '" /Fe:inverter_host_test.exe'
& $env:ComSpec /d /s /c $buildCommand
if($LASTEXITCODE -ne 0) { throw 'Host test build failed.' }
if($VirtualAngleOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --virtual-angle-only
} elseif($PllOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --pll-only
} elseif($AutoTestOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --auto-test-only
} elseif($AnnOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --ann-only
} elseif($CanOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --can-only
} elseif($AdcOnly) {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe') --adc-only
} else {
    & (Join-Path $buildCheckPath 'inverter_host_test.exe')
}
if($LASTEXITCODE -ne 0) { throw 'Host tests failed.' }
Write-Output "Host test executable: $buildCheckPath"
