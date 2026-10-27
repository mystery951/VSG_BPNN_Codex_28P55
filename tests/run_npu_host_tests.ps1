$ErrorActionPreference = 'Stop'
Write-Output 'NPU integration is archived. This compatibility entry point now runs CPU ANN regression tests.'
& (Join-Path $PSScriptRoot 'run_host_tests.ps1') -AnnOnly
if($LASTEXITCODE -ne 0) { throw 'CPU ANN regression tests failed.' }
