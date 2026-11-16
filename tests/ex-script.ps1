. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function Check($name,[scriptblock]$action){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $script:v=$null
 try{& $action;if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))};$results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"}
 catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{Stop-WV $script:v}
}
function SourceCase($name,$commands,$want,$undo="a`nb`nc`n",[Text.Encoding]$sourceEncoding=$utf8,[Text.Encoding]$docEncoding=$utf8){
 Check $name {
  $path=Join-Path $cases ($name+'.txt');$scriptPath=Join-Path $cases ($name+' commands.ex')
  [IO.File]::WriteAllText($path,"a`nb`nc`n",$docEncoding);[IO.File]::WriteAllText($scriptPath,$commands,$sourceEncoding)
  $script:v=Start-WV $path;Ex $v (':source "'+$scriptPath+'"');Wait-WVQuiet $v
  if([WV]::GetDlgItem($v.H,161) -ne [IntPtr]::Zero){throw 'Source unexpectedly opened input bar'}
  Save-WV $v;Equal-Bytes $path ($docEncoding.GetPreamble()+$docEncoding.GetBytes($want))
  if($null -ne $undo){Ex $v ':undo';Save-WV $v;Equal-Bytes $path ($docEncoding.GetPreamble()+$docEncoding.GetBytes($undo));Ex $v ':undo';Save-WV $v;Equal-Bytes $path ($docEncoding.GetPreamble()+$docEncoding.GetBytes($want))}
 }
}
SourceCase 'source-two-commands' "1s/a/A/`n2s/b/B/`n" "A`nB`nc`n" "A`nb`nc`n"
SourceCase 'source-comments-colons' "`"comment`n:`"comment`n  :1s/a/A/`n`n 2s/b/B/" "A`nB`nc`n" "A`nb`nc`n"
SourceCase 'source-append' "1append`nX`nY`n.`n" "a`nX`nY`nb`nc`n"
SourceCase 'source-insert' "2insert`nX`n.`n" "a`nX`nb`nc`n"
SourceCase 'source-change' "2,3change`nX`nY`n.`n" "a`nX`nY`n"
SourceCase 'source-empty-change' "2change`n.`n" "a`nc`n"
SourceCase 'source-eof-input' "1append`nX" "a`nX`nb`nc`n"
SourceCase 'source-literal-input' "1a`n`n .`n..`n:quit`n.`n" "a`n`n .`n..`n:quit`nb`nc`n"
SourceCase 'source-mixed-eols' "1a`rX`r`nY`n.`r" "a`nX`nY`nb`nc`n"
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 SourceCase ('source-'+$enc[0]) "1a`né中😀`n.`n" "a`né中😀`nb`nc`n" "a`nb`nc`n" $enc[1] ([Text.UTF8Encoding]::new($true))
}
SourceCase 'source-many-commands' (("set ignorecase`nset noignorecase`n")*1000) "a`nb`nc`n" $null
SourceCase 'source-large-input' ("1a`n"+('X'*131071)+"`n.`n") ("a`n"+('X'*131071)+"`nb`nc`n")
Check 'source-nested-repeat' {
 $path=Join-Path $cases 'nested.txt';$child=Join-Path $cases 'nested child.ex';$parent=Join-Path $cases 'nested parent.ex'
 [IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);[IO.File]::WriteAllText($child,"1s/^/X/`n",$utf8)
 [IO.File]::WriteAllText($parent,((('source "'+$child+'"')+"`n")*2),$utf8)
 $script:v=Start-WV $path;Ex $v (':so "'+$parent+'"');Wait-WVQuiet $v;Save-WV $v;Equal-Bytes $path $utf8.GetBytes("XXa`nb`nc`n")
 Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("Xa`nb`nc`n")
}
foreach($nested in @($false,$true)){
 Check ('source-write-quit-'+$nested) {
  $path=Join-Path $cases ('quit-'+$nested+'.txt');$child=Join-Path $cases ('quit-child-'+$nested+'.ex');$parent=Join-Path $cases ('quit-parent-'+$nested+'.ex')
  [IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);[IO.File]::WriteAllText($child,"1s/a/A/`nwrite`nquit`n",$utf8)
  $source=$child
  if($nested){[IO.File]::WriteAllText($parent,('source "'+$child+'"'+"`n2s/b/B/`nwrite`n"),$utf8);$source=$parent}
  $script:v=Start-WV $path;Menu $v 119;Chars $v ':';[WV]::SetText([WV]::GetDlgItem($v.H,161),(':source "'+$source+'"'))
  [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  if(!$v.P.WaitForExit(15000)){throw 'Source write/quit did not exit'}
  if($v.P.ExitCode){throw ('Source write/quit exit code '+$v.P.ExitCode)}
  Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
 }
}
Check 'shell-configured-executable' {
 $path=Join-Path $cases 'shell.txt';$folder=Join-Path $cases 'shell folder';New-Item -ItemType Directory -Force $folder|Out-Null
 $fixture=Join-Path $folder 'shell-fixture.exe';Copy-Item "$PSScriptRoot/shell-fixture.exe" $fixture
 $record=Join-Path $cases 'shell-launch.txt';$saved=$env:WINVI_TEST_SHELL_RECORD
 try{
  $env:WINVI_TEST_SHELL_RECORD=$record;[IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);$script:v=Start-WV $path
  Ex $v (':set shell="'+$fixture+'"');Ex $v ':1s/a/A/';Ex $v ':sh'
  $end=(Get-Date).AddSeconds(15);while(!(Test-Path $record) -and (Get-Date) -lt $end){Start-Sleep -Milliseconds 50}
  if(!(Test-Path $record)){throw 'Configured interactive shell did not run'}
  $line=[Text.Encoding]::Unicode.GetString([IO.File]::ReadAllBytes($record));if($line.StartsWith('"\\?\')){$line='"'+$line.Substring(5)};if($line -cne ('"'+$fixture+'"')){throw ('Unexpected shell command line: '+$line)}
  Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n");Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
 }finally{
  $env:WINVI_TEST_SHELL_RECORD=$saved
  # This dummy instance must not persist the fixture shell into user settings.
  if($script:v -and !$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}
 }
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-script-results.json"
if(@($results|Where-Object {$_.Result -eq 'FAIL'}).Count){throw 'Ex source/shell application regressions failed'}
"$($results.Count) source/shell application cases passed"
