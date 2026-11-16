. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($true)
function Posted($v,$text){
 foreach($c in $text.ToCharArray()){
  if(![WV]::PostMessageW($v.H,0x102,[UIntPtr]::new([uint64][char]$c),[IntPtr]::new(1))){throw 'Cannot post mapping input'}
 }
 Wait-WVQuiet $v
}
function Check($name,$commands,$keys,$expected){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null
 try{
  $path=Join-Path $cases ($name+'.txt')
  [IO.File]::WriteAllText($path,"abc`ndef`n",$utf8)
  $v=Start-WV $path
  foreach($command in $commands){Ex $v $command}
  if($keys -is [scriptblock]){& $keys $v;Wait-WVQuiet $v}else{Posted $v $keys}
  Save-WV $v
  Equal-Bytes $path ($utf8.GetPreamble()+$utf8.GetBytes($expected))
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{
  $results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"
 }finally{
  # Each dummy process owns its mappings; avoid storing them in user preferences.
  if($v -and !$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}
 }
}
Check 'abbreviation-insert' @(':ab btw by the way') ("ibtw "+[char]27) "by the way abc`ndef`n"
Check 'abbreviation-redefine' @(':ab btw wrong',':abbreviate btw right') ("ibtw "+[char]27) "right abc`ndef`n"
Check 'abbreviation-remove' @(':ab btw X',':una btw') ("ibtw "+[char]27) "btw abc`ndef`n"
Check 'abbreviation-word-boundary' @(':ab btw X') ("ixbtw "+[char]27) "xbtw abc`ndef`n"
Check 'abbreviation-word-extends' @(':ab btw X') ("ibtwx "+[char]27) "btwx abc`ndef`n"
Check 'abbreviation-consecutive' @(':ab btw X') ("ibtw btw "+[char]27) "X X abc`ndef`n"
Check 'abbreviation-escape' @(':ab btw X') ("ibtw"+[char]27) "Xabc`ndef`n"
Check 'abbreviation-punctuation' @(':ab btw X') ("ibtw,btw."+[char]27) "X,X.abc`ndef`n"
Check 'abbreviation-prefix' @(':ab abc LONG',':ab ab SHORT') ("iabc "+[char]27) "LONG abc`ndef`n"
Check 'abbreviation-command-entry' @(':ab sub 1s/a/A/') {
 param($v)
 Chars $v ':'
 $edit=[WV]::GetDlgItem($v.H,161)
 if($edit-eq[IntPtr]::Zero){throw 'Missing command edit'}
 foreach($c in ("sub "+[char]13).ToCharArray()){
  [void][WV]::PostMessageW($edit,0x102,[UIntPtr]::new([uint64][char]$c),[IntPtr]::new(1))
 }
} "Abc`ndef`n"
$unicode=[string][char]0x4e2d+[char]0xd83d+[char]0xde00
Check 'abbreviation-unicode-rhs' @(':ab btw '+$unicode) ("ibtw "+[char]27) ($unicode+" abc`ndef`n")
Check 'abbreviation-unicode-lhs' @(':ab '+[char]0x4e2d+' X') ('i'+[char]0x4e2d+' '+[char]27) "X abc`ndef`n"
Check 'abbreviation-normal-mode-unchanged' @(':ab x Z') 'x' "bc`ndef`n"
Check 'abbreviation-literal-hash' @(':ab #1 X') ('i#1 '+[char]27) "X abc`ndef`n"
$emoji=[string][char]0xd83d+[char]0xde00
Check 'abbreviation-emoji-lhs' @(':ab '+$emoji+' X') ('i'+$emoji+' '+[char]27) "X abc`ndef`n"
Check 'abbreviation-long-word' @((':ab '+('a'*63)+' X')) ('i'+('a'*64)+' '+[char]27) (('a'*64)+" abc`ndef`n")
Check 'abbreviation-burst' @(':ab btw X') ('i'+('btw '*100)+[char]27) (('X '*100)+"abc`ndef`n")
Check 'abbreviation-undo' @(':ab btw X') ("ibtw "+[char]27+'u') "abc`ndef`n"
Check 'abbreviation-reject-bang' @(':ab btw X',':ab! btw Y') ("ibtw "+[char]27) "X abc`ndef`n"
Check 'abbreviation-reject-address' @(':ab btw X',':1ab btw Y') ("ibtw "+[char]27) "X abc`ndef`n"
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-abbreviation-results.json"
if(@($results|Where-Object {$_.Result -eq 'FAIL'}).Count){throw 'Ex abbreviation application regressions failed'}
"$($results.Count) abbreviation application cases passed"