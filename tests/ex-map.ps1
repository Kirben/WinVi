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
Check 'map-normal' @(':map Q rZ') 'Q' "Zbc`ndef`n"
Check 'map-redefine' @(':map Q rX',':map Q rZ') 'Q' "Zbc`ndef`n"
Check 'map-unmap' @(':map x rZ',':unmap x') 'x' "bc`ndef`n"
Check 'map-undo' @(':map Q rZ') 'Qu' "abc`ndef`n"
Check 'map-nop' @(':map x <Nop>') 'x' "abc`ndef`n"
Check 'map-multiple' @(':map zz rZ') 'zz' "Zbc`ndef`n"
Check 'unmapped-prefix-control' @() 'zx' "abc`ndef`n"
Check 'map-mismatch' @(':map zz rZ') 'zx' "abc`ndef`n"
Check 'map-input' @(':map! jj <Esc>') 'ijjx' "bc`ndef`n"
Check 'map-mode-split' @(':map Q rZ',':map! Q X',':unmap! Q') 'Q' "Zbc`ndef`n"
Check 'map-self-remove' @(':map Q :unmap Q<CR>rZ') 'Q' "Zbc`ndef`n"
Check 'map-self-redefine' @(':map Q :map Q rX<CR>rZ') 'QQ' "Xbc`ndef`n"
Check 'map-command-entry' @(':map! ZZ 1s/a/A/<CR>') ':ZZ' "Abc`ndef`n"
Check 'map-function-key' @(':map #24 rZ') {
 param($v)
 [void][WV]::PostMessageW($v.H,0x100,[UIntPtr]::new(135),[IntPtr]::new(1))
 [void][WV]::PostMessageW($v.H,0x101,[UIntPtr]::new(135),[IntPtr]::new(1))
} "Zbc`ndef`n"
$unicode=[string][char]0x4e2d+[char]0xd83d+[char]0xde00
Check 'map-unicode-output' @(':map Q i'+$unicode+'<Esc>') 'Q' ($unicode+"abc`ndef`n")
Check 'map-unicode-input' @(':map '+[char]0x4e2d+' rZ') ([string][char]0x4e2d) "Zbc`ndef`n"
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-map-results.json"
if(@($results|Where-Object {$_.Result -eq 'FAIL'}).Count){throw 'Ex mapping application regressions failed'}
"$($results.Count) mapping application cases passed"
