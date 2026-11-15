. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($true)
function Edit-Case($name,$inputText,$command,$expected,[Text.Encoding]$encoding=$utf8){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null
 try{
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$inputText,$encoding)
  $original=[IO.File]::ReadAllBytes($path);$v=Start-WV $path
  $phase='substitute';Ex $v ':set magic';Ex $v $command;Wait-WVQuiet $v;Save-WV $v
  $want=$encoding.GetPreamble()+$encoding.GetBytes($expected);Equal-Bytes $path $want
  $phase='undo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $original
  $phase='redo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $want
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name} ($phase): $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
Edit-Case 'numeric-byte-classes' "•`n" ':%s/[\%e2][\%80][\%a2]/fixed/' "fixed`n"
Edit-Case 'repeated-alternative' "a;`n" ':%s/\(.*\|.*\);/fixed/' "fixed`n" ([Text.UTF8Encoding]::new($false))
Edit-Case 'ungrouped-alternative' "b`n" ':%s/a\|b/fixed/' "fixed`n"
Edit-Case 'boundary-alternative' "xy `n" ':%s/\(x\|xy\)\>/fixed/' "fixed `n"
Edit-Case 'capture-after-retry' "xyz`n" ':%s/\(x\|xy\)z/<\1>/' "<xy>`n"
Edit-Case 'repeat-prefix-backtracking' "aaa`n" ':%s/^a\(a*\|q\)$/fixed/' "fixed`n" ([Text.UTF8Encoding]::new($false))
Edit-Case 'root-capture-numbering' "bc`n" ':%s/a\|\(bc\)/<\1>/' "<bc>`n"
Edit-Case 'global-root-alternative' "a`nkeep`nb`n" ':g/a\|b/d' "keep`n"
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/search-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Search regressions failed'}
"$($results.Count) search cases passed (including undo and redo)"
