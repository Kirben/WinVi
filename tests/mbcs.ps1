. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new()
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)
function Check($name,$source,$keys,$expected,[bool]$undo=$true){
 if($env:WINVI_TEST_FILTER-and$name-notmatch$env:WINVI_TEST_FILTER){return}
 $v=$null
 try{
  $path=Join-Path $cases ($name+'.txt')
  $original=if($source-is[byte[]]){$source}else{$enc.GetBytes($source)}
  $want=if($expected-is[byte[]]){$expected}else{$enc.GetBytes($expected)}
  [IO.File]::WriteAllBytes($path,$original);$v=Start-WV $path
  Ex $v (':set ansicodepage='+$cp+' oemcodepage='+$cp);Menu $v $mode
  if($keys-is[scriptblock]){& $keys $v}elseif($keys.StartsWith(':')){Ex $v $keys}else{Chars $v $keys}
  Wait-WVQuiet $v;Save-WV $v;Equal-Bytes $path $want
  if($undo){Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original;Ex $v ':undo';Save-WV $v;Equal-Bytes $path $want}
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{if($v-and!$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}}
}
foreach($cp in 932,950){
 $enc=[Text.Encoding]::GetEncoding($cp,[Text.EncoderFallback]::ExceptionFallback,[Text.DecoderFallback]::ExceptionFallback)
 $word=if($cp-eq932){[string][char]0x65e5+[char]0x672c}else{[string][char]0x4e2d+[char]0x6587}
 $first=$word.Substring(0,1);$second=$word.Substring(1,1);$esc=[string][char]27
 foreach($mode in 152,151){
  $prefix="$cp-$(if($mode-eq152){'ansi'}else{'oem'})"
  Check "$prefix-insert" "X`n" ('i'+$word+$esc) ($word+"X`n")
  Check "$prefix-delete" ($word+"X`n") 'x' ($second+"X`n")
  Check "$prefix-ascii-delete-then-move" ("A"+$word+"X`n") 'xlx' ($first+"X`n") $false
  Check "$prefix-double-byte-replacement" ($word+"`n") (':s/'+$first+'/'+$word+$word+'/') ($word+$word+$second+"`n")
  Check "$prefix-read-splices-pages" "X`n" {
   param($v)
   $inputPath=Join-Path $cases ($prefix+'-read-input.txt');[IO.File]::WriteAllBytes($inputPath,$enc.GetBytes($word+"`n"));Ex $v (':read '+$inputPath)
  } ("X`n"+$word+"`n")
  Check "$prefix-delete-two" ($word+"X`n") '2x' "X`n"
  Check "$prefix-move-delete" ($word+"X`n") 'lx' ($first+"X`n")
  Check "$prefix-backward-delete" ($word+"X`n") '2lhx' ($first+"X`n")
  Check "$prefix-replace-single" "AB`n" ('r'+$first) ($first+"B`n")
  Check "$prefix-replace-pair" ($word+"X`n") ('rZ') ('Z'+$second+"X`n")
  Check "$prefix-replace-mode" "ABX`n" ('R'+$word+$esc) ($word+"X`n")
  Check "$prefix-backspace" "X`n" ('i'+$word+[char]8+$esc) ($first+"X`n")
  Check "$prefix-existing-backspace" ($word+"X`n") ('li'+[char]8+$esc) ($second+"X`n")
  Check "$prefix-literal-substitute" ($word+$word+"`n") (':%s/'+$first+'/Z/g') ('Z'+$second+'Z'+$second+"`n")
  Check "$prefix-dot-substitute" ($first+"`n") ':s/^.$/X/' "X`n"
  Check "$prefix-class-substitute" ($word+"`n") (':s/['+$word+']/X/g') "XX`n"
  Check "$prefix-word-motion" ($word+" X`n") 'wx' ($word+" `n")
  $fullwidth=[string][char]0xff21
  Check "$prefix-replacement-case" ($fullwidth+"X`n") ':s/./\L&/' ([string][char]0xff41+"X`n")
  Check "$prefix-case-conversion" ($fullwidth+"X`n") '~' ([string][char]0xff41+"X`n")
  Check "$prefix-split-page-delete" (('a'*4095)+$first+"Z`n") '4095lx' (('a'*4095)+"Z`n")
  Check "$prefix-split-page-backspace" (('a'*4095)+$first+"Z`n") ('4096li'+[char]8+$esc) (('a'*4095)+"Z`n")
  $trail=$enc.GetString([byte[]]@($(if($cp-eq932){0x95}else{0xa4}),0x5c))
  Check "$prefix-ascii-trail-not-a-match" ($trail+"X`n") ':s/\\/Z/g' ($trail+"X`n") $false
  Check "$prefix-append-ex" "X`n" {
   param($v)
   Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161);[WV]::SetText($edit,':append')
   [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
   $limit=(Get-Date).AddSeconds(10)
   while([WV]::Text($v.H)-notmatch'ex append input'){
    if((Get-Date)-gt$limit){throw 'Missing ex input bar'};Start-Sleep -Milliseconds 50
   }
   $edit=[WV]::GetDlgItem($v.H,161);[WV]::SetText($edit,$word);[void][WV]::Send($edit,0x102,13,0)
   [WV]::SetText($edit,'.');[void][WV]::Send($edit,0x102,13,0)
  } ("X`n"+$word+"`n")
  Check "$prefix-unrepresentable-keeps-bytes" "X`n" ('i'+[char]::ConvertFromUtf32(0x1f600)+$esc) "X`n" $false
  Check "$prefix-malformed-roundtrip" ([byte[]]@(0x81,0x20,0x81)) '' ([byte[]]@(0x81,0x20,0x81)) $false
  Check "$prefix-malformed-delete-undo" ([byte[]]@(0x81,0x20,0x81)) 'x' ([byte[]]@(0x20,0x81))
 }
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/mbcs-results.json"
if(@($results|Where-Object {$_.Result-eq'FAIL'}).Count){throw 'MBCS application regressions failed'}
"$($results.Count) MBCS application cases passed"
