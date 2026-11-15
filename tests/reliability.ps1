. "$PSScriptRoot\common.ps1"
$fixture=Join-Path $PSScriptRoot 'command-fixture.exe'
$results=[Collections.Generic.List[object]]::new()
function Run-Case($name,[scriptblock]$test){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $script:v=$null;$clock=[Diagnostics.Stopwatch]::StartNew()
 try{& $test;if($script:v -and !$script:v.P.HasExited -and (Get-Item $script:v.Log).Length){throw ([IO.File]::ReadAllText($script:v.Log))};$results.Add([pscustomobject]@{Test=$name;Result='PASS';Milliseconds=$clock.ElapsedMilliseconds});"PASS $name"}
 catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL $name : $($_.Exception.Message)"}
 finally{Stop-WV $script:v}
}
function Open-Case($name){$path=Join-Path $cases "$name.txt";[IO.File]::WriteAllText($path,"seed`r`n",[Text.UTF8Encoding]::new($false));$script:v=Start-WV $path;return $path}
function Command($mode){Ex $v ('!"'+$fixture+'" '+$mode)}
function Cancel-Command{[void][WV]::PostMessageW($v.H,0x102,[UIntPtr]3,[IntPtr]1);Start-Sleep -Milliseconds 150;Wait-WVCommand $v 15}
function Recover-And-Save($path){Ex $v ':1';Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v;if([IO.File]::ReadAllBytes($path)[0] -ne 88){throw 'Editor did not accept the recovery edit'}}
function Wait-Pid($path){$limit=(Get-Date).AddSeconds(8);while(!(Test-Path $path)){if((Get-Date)-gt$limit){throw "Fixture did not start: $path"};Start-Sleep -Milliseconds 50};$pidValue=[int](Get-Content $path -Raw);return [Diagnostics.Process]::GetProcessById($pidValue)}
Run-Case 'command-normal-output-and-repeated-launch' {
 $path=Open-Case 'normal-command';for($i=0;$i-lt 12;$i++){Ex $v ':1';Command 'emit';Wait-WVQuiet $v 10}
 Save-WV $v
 Equal-Bytes $path ([Text.Encoding]::ASCII.GetBytes(("OUT`r`n"*12)+"seed`r`n"))
}
Run-Case 'command-input-filter' {
 $path=Open-Case 'filter-command';Menu $v 120;Command 'echo';Wait-WVQuiet $v 10;Save-WV $v
 Equal-Bytes $path ([Text.Encoding]::ASCII.GetBytes("seed`r`n"))
}
Run-Case 'cancel-blocked-input-and-process-tree' {
 $path=Join-Path $cases 'blocked-command.txt';[IO.File]::WriteAllText($path,('a'*1048576)+"`r`n");$script:v=Start-WV $path;Menu $v 120;$parent=Join-Path $cases 'parent.pid';$child=Join-Path $cases 'child.pid'
 foreach($p in $parent,$child){if(Test-Path $p){Remove-Item -LiteralPath $p}}
 Command ('tree "'+$parent+'" "'+$child+'"');$parentProcess=Wait-Pid $parent;$childProcess=Wait-Pid $child
 Cancel-Command
 if(!$parentProcess.WaitForExit(3000)-or!$childProcess.WaitForExit(3000)){throw 'Command tree survived cancellation'}
 Recover-And-Save $path
}
Run-Case 'cancel-paused-output' {
 $path=Open-Case 'paused-command';$pidFile=Join-Path $cases 'flood.pid';if(Test-Path $pidFile){Remove-Item -LiteralPath $pidFile}
 Command ('flood "'+$pidFile+'"');$producer=Wait-Pid $pidFile
 [void][WV]::Send($v.H,0x102,19,1);Start-Sleep -Milliseconds 600
 Cancel-Command;if(!$producer.WaitForExit(3000)){throw 'Paused producer survived cancellation'};Recover-And-Save $path
}
Run-Case 'close-window-during-command' {
 [void](Open-Case 'close-command');$pidFile=Join-Path $cases 'close.pid';if(Test-Path $pidFile){Remove-Item -LiteralPath $pidFile}
 Command ('wait "'+$pidFile+'"');$producer=Wait-Pid $pidFile
 [void][WV]::PostMessageW($v.H,0x10,[UIntPtr]0,[IntPtr]0)
 if(!$v.P.WaitForExit(5000)-or$v.P.ExitCode-ne0){throw 'Editor did not close cleanly'}
 if(!$producer.WaitForExit(3000)){throw 'Command survived editor close'}
}
Run-Case 'inactive-watcher-switch-and-close' {
 $path=Open-Case 'watch-inactive';$next=Join-Path $cases 'watch-next.txt';[IO.File]::WriteAllText($next,"next`r`n")
 for($i=0;$i-lt 10;$i++){
  [IO.File]::SetLastWriteTimeUtc($path,[DateTime]::UtcNow.AddMinutes($i+1))
  Start-Sleep -Milliseconds 120;Ex $v (':e! '+$next);Ex $v (':e! '+$path)
 }
 [void][WV]::PostMessageW($v.H,0x10,[UIntPtr]0,[IntPtr]0)
 if(!$v.P.WaitForExit(5000)-or$v.P.ExitCode-ne0){throw 'Watcher prevented clean editor close'}
}
$results|ConvertTo-Json -Depth 4|Set-Content (Join-Path $PSScriptRoot 'reliability-results.json')
if(@($results|Where-Object Result -ne 'PASS').Count){exit 1}
