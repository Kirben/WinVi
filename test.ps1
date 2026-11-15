param(
 [ValidateSet('Release','Asan','Both')][string]$Configuration='Both',
 [switch]$SkipBuild,
 [switch]$LargeFiles,
 [ValidateRange(1,64)][int]$LargeGiB=17,
 [switch]$DenseLines
)
$ErrorActionPreference='Stop'
$root=$PSScriptRoot;$tests=Join-Path $root 'tests'
$results=Join-Path $tests ('results/'+(Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force $results|Out-Null
$manifest=[Collections.Generic.List[object]]::new()
$saved=@{};foreach($key in 'WINVI_OUTPUT_NAME','WINVI_TEST_EXE','WINVI_TEST_CASES','WINVI_TEST_FILTER','WINVI_BUILD_FLAGS'){$saved[$key]=[Environment]::GetEnvironmentVariable($key)}
function Run-Step($name,[scriptblock]$action,$folder){
 $watch=[Diagnostics.Stopwatch]::StartNew()
 try {& $action 2>&1|Tee-Object (Join-Path $folder "$name.log")|Write-Host
 if($LASTEXITCODE){throw "$name failed (exit $LASTEXITCODE)"}}catch{$manifest.Add([pscustomobject]@{Step=$name;Result="FAIL";Error=$_.Exception.Message});throw}
 $manifest.Add([pscustomobject]@{Step=$name;Result='PASS';Seconds=[math]::Round($watch.Elapsed.TotalSeconds,2)})
}
try {
 $env:WINVI_TEST_FILTER='';$env:WINVI_BUILD_FLAGS=''
 if(!$SkipBuild){
  if($Configuration -in 'Both','Release'){$env:WINVI_OUTPUT_NAME='WinVi64-reliable.exe';Run-Step 'build-release' {& "$root/build-x64.cmd"} $results}
  if($Configuration -in 'Both','Asan'){Run-Step 'build-asan' {& "$root/build-asan.cmd"} $results}
 }
 Run-Step 'build-command-fixture' {& "$tests/command-fixture.cmd"} $results
 foreach($native in 'format','platform','size','io','page-fault','hex-layout','hex-print'){Run-Step "native-$native" {& "$tests/$native-test.cmd"} $results}
 Run-Step 'focused-asan-regressions' {& "$tests/bugs/build-and-test.cmd"} $results
 Copy-Item "$tests/bugs/bug-results.json" "$results/focused-results.json"
 Copy-Item "$tests/bugs/after-command-faults.stdout.txt" "$results/resource-fault-details.txt"
 $kinds=if($Configuration-eq'Both'){@('Release','Asan')}else{@($Configuration)}
 foreach($kind in $kinds){
  $folder=Join-Path $results $kind;New-Item -ItemType Directory -Force $folder|Out-Null
  $env:WINVI_TEST_EXE=Join-Path $root $(if($kind-eq'Release'){'x64/WinVi64-reliable.exe'}else{'asan/WinVi64.exe'})
  if(!(Test-Path $env:WINVI_TEST_EXE)){throw "Missing executable $env:WINVI_TEST_EXE"}
  $env:WINVI_TEST_CASES=Join-Path $folder 'cases'
  Run-Step "$kind-save-faults" {if($kind-eq'Asan'){& "$tests/safe-save-test.cmd" asan}else{& "$tests/safe-save-test.cmd"}} $folder
  foreach($suite in 'integration','read-failure','search-cancel','reliability'){
   Run-Step "$kind-$suite" {& pwsh -NoProfile -File "$tests/$suite.ps1"} $folder
   Copy-Item "$tests/$suite-results.json" "$folder/$suite-results.json"
  }
  if($LargeFiles){
   Run-Step "$kind-large" {if($DenseLines){& pwsh -NoProfile -File "$tests/large-audit.ps1" -GiB $LargeGiB -DenseLines}else{& pwsh -NoProfile -File "$tests/large-audit.ps1" -GiB $LargeGiB}} $folder
   Copy-Item "$tests/large-$LargeGiB-GiB-results.json" "$folder/large-results.json"
  }
  [pscustomobject]@{Executable=$env:WINVI_TEST_EXE;SHA256=(Get-FileHash $env:WINVI_TEST_EXE).Hash;Target='Windows 10/11 x64';Runtime=[Environment]::OSVersion.VersionString}|ConvertTo-Json|Set-Content "$folder/build.json"
 }
 "PASS: unified suite completed. Results: $results"
}finally {
 $manifest|ConvertTo-Json -Depth 4|Set-Content "$results/manifest.json"
 foreach($key in $saved.Keys){[Environment]::SetEnvironmentVariable($key,$saved[$key])}
}
