param([switch]$LaunchPad, [switch]$Flash, [string]$ExportDirectory = '')
$ErrorActionPreference = 'Stop'
$projectPath = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path ([IO.Path]::GetTempPath()) ('vsg-ann-target-' + [guid]::NewGuid().ToString('N'))
$syscfgPath = Join-Path $buildPath 'syscfg'
New-Item -ItemType Directory -Path $syscfgPath | Out-Null
$compilerRoot = 'D:/Engineering_software/ti/CCS/CCS2051/ccs/tools/compiler/ti-cgt-c2000_25.11.0.LTS'
$sdkPath = 'C:/ti/C2000Ware_26_01_00_00'
$syscfgCli = 'D:/Engineering_software/ti/CCS/CCS2051/ccs/utils/sysconfig_1.27.1/sysconfig_cli.bat'
& $syscfgCli -s "$sdkPath/.metadata/sdk.json" -d F28P55x -p 100PZ -r F28P55x_100PZ --script "$projectPath/c2000.syscfg" -o $syscfgPath --compiler ccs
if($LASTEXITCODE -ne 0) { throw 'SysConfig failed.' }
$flags = @('-v28','-ml','-mt','--cla_support=cla2','--float_support=fpu32','--tmu_support=tmu1','--vcu_support=vcrc','-O2','--fp_mode=relaxed','--abi=eabi','--gen_func_subsections=on','--diag_suppress=10063','--diag_warning=225','--diag_wrap=off','--display_error_number','--define=RAM','--define=DEBUG',
    "--include_path=$projectPath","--include_path=$projectPath/device","--include_path=$sdkPath","--include_path=$sdkPath/driverlib/f28p55x/driverlib","--include_path=$compilerRoot/include","--include_path=$syscfgPath","--cmd_file=$syscfgPath/board.opt","--cmd_file=$syscfgPath/c2000ware_libraries.opt")
if($LaunchPad) { $flags += '--define=_LAUNCHXL_F28P55X' }
if($Flash) {
    $flags = @($flags | Where-Object { $_ -ne '--define=RAM' })
    $flags += '--define=_FLASH'
}
$sources = @(Get-ChildItem -LiteralPath "$projectPath/app" -Recurse -Filter '*.c' | Where-Object { $_.Name -ne 'ann_npu.c' } | ForEach-Object { $_.FullName })
$sources += @("$projectPath/empty_driverlib_main.c","$projectPath/device/device.c","$projectPath/device/f28p55x_codestartbranch.asm","$syscfgPath/board.c","$syscfgPath/c2000ware_libraries.c")
$objects = @()
foreach($source in $sources) {
    $object = Join-Path $buildPath (([IO.Path]::GetFileNameWithoutExtension($source)) + '.obj')
    & "$compilerRoot/bin/cl2000.exe" @flags "--output_file=$object" -c $source
    if($LASTEXITCODE -ne 0) { throw "Target compile failed: $source" }
    $objects += $object
}
$memoryDefine = if($Flash) { '--define=_FLASH' } else { '--define=RAM' }
$linkerPath = if($Flash) { "$projectPath/28p55x_generic_flash_lnk.cmd" } else { "$projectPath/28p55x_generic_ram_lnk.cmd" }
$stackSize = if($LaunchPad) { '--stack_size=0x3F8' } else { '--stack_size=0x200' }
$linkFlags = @('-v28','-ml','-mt','--float_support=fpu32','--abi=eabi',$memoryDefine)
if($LaunchPad) { $linkFlags += '--define=_LAUNCHXL_F28P55X' }
$linkFlags += @('-z','--rom_model','--entry_point=code_start','--diag_suppress=10063',$stackSize,'--heap_size=0x200','--warn_sections','--diag_wrap=off','--display_error_number',
    "--search_path=$compilerRoot/lib","--search_path=$syscfgPath","--map_file=$buildPath/inverter.map","--xml_link_info=$buildPath/inverter_link.xml","--output_file=$buildPath/inverter.out")
& "$compilerRoot/bin/cl2000.exe" @linkFlags @objects $linkerPath "$sdkPath/driverlib/f28p55x/driverlib/ccs/Debug/driverlib.lib" '-lc2000ware_libraries.cmd.genlibs' '-llibc.a'
if($LASTEXITCODE -ne 0) { throw 'Target link failed.' }
if(Select-String -LiteralPath "$buildPath/inverter.map" -Pattern 'tvmgen_default|npu_model[/\\]mod\.a|ann_npu\.obj' -Quiet) {
    throw 'CPU-only build unexpectedly links NPU code.'
}
Write-Output "Target build passed (LaunchPad=$LaunchPad, Flash=$Flash): $buildPath/inverter.out"
Select-String -LiteralPath "$buildPath/inverter.map" -Pattern '^\.ann_weights','^\.const','^\.bss','^\.stack' -Context 0,1 | ForEach-Object { $_.Line; $_.Context.PostContext }
if($ExportDirectory) {
    $exportPath = [IO.Path]::GetFullPath((Join-Path $projectPath $ExportDirectory))
    if(-not $exportPath.StartsWith($projectPath + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Export directory must stay within the project.'
    }
    New-Item -ItemType Directory -Path $exportPath -Force | Out-Null
    Copy-Item -LiteralPath "$buildPath/inverter.out" -Destination (Join-Path $exportPath 'VSG_BPNN_Codex_28P55.out')
    Copy-Item -LiteralPath "$buildPath/inverter.map" -Destination (Join-Path $exportPath 'VSG_BPNN_Codex_28P55.map')
    Write-Output "Exported binary/map: $exportPath"
}

