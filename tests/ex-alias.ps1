. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function Submit($v,$command){
 Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161)
 if($edit-eq[IntPtr]::Zero){throw 'Missing command edit'}
 [WV]::SetText($edit,$command)
 if(![WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)){throw 'Cannot submit command'}
}
function Exited($v){
 if(!$v.P.WaitForExit(15000)){throw 'Alias did not exit'}
 if($v.P.ExitCode){throw ('Unexpected exit code '+$v.P.ExitCode)}
}
function Wait-Dialog($v){
 $until=(Get-Date).AddSeconds(10)
 do{$dialog=Dialog $v;if($dialog-ne[IntPtr]::Zero){return $dialog};if($v.P.HasExited){throw 'Editor exited unexpectedly'};Start-Sleep -Milliseconds 50}while((Get-Date)-lt$until)
 throw 'Expected dialog did not appear'
}
function Check($name,[scriptblock]$action){
 if($env:WINVI_TEST_FILTER-and$name-notmatch$env:WINVI_TEST_FILTER){return}
 $script:v=$null
 try{
  $script:folder=Join-Path $cases ($name+'-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force $folder|Out-Null
  $script:path=Join-Path $folder 'original.txt';$script:other=Join-Path $folder 'target with spaces.txt'
  [IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);[IO.File]::WriteAllText($other,"target`n",$utf8)
  if($name-eq'exit-clean-does-not-write'){[IO.File]::SetLastWriteTimeUtc($path,[datetime]::new(2001,2,3,4,5,6,[DateTimeKind]::Utc))}
  if($name-eq'exit-force-readonly'){[IO.File]::SetAttributes($path,[IO.FileAttributes]::ReadOnly)}
  $script:v=Start-WV $path;& $action
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{if($v-and!$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}}
}
Check 'ex-opens-current-window' {
 $pidBefore=$v.P.Id;Ex $v (':ex '+$other)
 if($v.P.HasExited-or$v.P.Id-ne$pidBefore-or[WV]::Text($v.H)-notmatch'target with spaces'){throw 'Ex did not open the target in the same window'}
 Ex $v ':1s/target/changed/';Save-WV $v;Equal-Bytes $other $utf8.GetBytes("changed`n")
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
Check 'ex-force-discards' {
 Ex $v ':1s/a/A/';Ex $v (':ex! '+$other)
 if([WV]::Text($v.H)-notmatch'target with spaces'){throw 'Force ex did not switch buffers'}
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
Check 'ex-force-reloads-current' {
 Ex $v ':1s/a/A/';Ex $v ':ex!';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
Check 'ex-unsaved-cancel' {
 Ex $v ':1s/a/A/';Submit $v (':ex '+$other);$dialog=Wait-Dialog $v
 [void][WV]::Send($dialog,0x111,2,0);Wait-WVQuiet $v
 if([WV]::Text($v.H)-notmatch'original'){throw 'Cancelled ex changed the current file'}
 Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'ex-rejects-address' {
 Ex $v ':1s/a/A/';Ex $v (':1ex! '+$other)
 if([WV]::Text($v.H)-notmatch'original'){throw 'Addressed ex replaced the current buffer'}
 Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'exit-clean-does-not-write' {
 $stamp=[IO.File]::GetLastWriteTimeUtc($path)
 Submit $v ':exit';Exited $v
 if([IO.File]::GetLastWriteTimeUtc($path)-ne$stamp){throw 'Clean exit rewrote the file'}
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
foreach($command in ':exit',':exi',':exit!'){
 Check ('exit-modified-'+$command.Replace(':','').Replace('!','force')) {
  Ex $v ':1s/a/A/';Submit $v $command;Exited $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
 }
}
Check 'exit-force-readonly' {
 Ex $v ':1s/a/A/';Submit $v ':exit!';Exited $v
 Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'exit-rejects-partial-range' {
 Ex $v ':1s/a/A/';Ex $v ':1exit'
 if($v.P.HasExited){throw 'Partial-range exit closed the editor'}
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n");Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'exit-failed-save-stays-open' {
 Ex $v ':1s/a/A/';$locked=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 try{
  Submit $v ':exit';$dialog=Wait-Dialog $v
  [void][WV]::PostMessageW($dialog,0x10,[UIntPtr]::Zero,[IntPtr]::Zero);Wait-WVQuiet $v
  if($v.P.HasExited){throw 'Exit closed the editor after a failed save'}
  Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
 }finally{$locked.Dispose()}
 Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-alias-results.json"
if(@($results|Where-Object {$_.Result-eq'FAIL'}).Count){throw 'Ex alias application regressions failed'}
"$($results.Count) alias application cases passed"
