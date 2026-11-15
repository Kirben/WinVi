$ErrorActionPreference='Stop'
$exe=Join-Path $PSScriptRoot 'review-probes.exe'
$results=foreach($case in 'wildcard','popup','search-oom','search-cancel','search-depth','wildcard-oracle','command-faults','command-ui-shutdown'){
 $watch=[Diagnostics.Stopwatch]::StartNew()
 $p=Start-Process -FilePath $exe -ArgumentList "--$case" -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -RedirectStandardOutput "$PSScriptRoot/after-$case.stdout.txt" -RedirectStandardError "$PSScriptRoot/after-$case.stderr.txt" -PassThru
 $timeout=!$p.WaitForExit(5000)
 if($timeout){$p.Kill();$p.WaitForExit()}
 if($timeout -or $p.ExitCode){throw "$case failed: timeout=$timeout exit=$($p.ExitCode)"}
 [pscustomobject]@{Case=$case;Result='PASS';Milliseconds=$watch.ElapsedMilliseconds}
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/bug-results.json"
$results|Format-Table
Push-Location $PSScriptRoot
try{& $exe;if($LASTEXITCODE){throw 'Existing security regressions failed'}}finally{Pop-Location}
