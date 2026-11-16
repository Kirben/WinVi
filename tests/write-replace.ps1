. "$PSScriptRoot/common.ps1"
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)
# Long-line repaint/undo can exceed SendMessageTimeout's two-second budget.
# Post the command and wait for completion/idle instead of retrying an edit.
function Ex($v,$command){
 Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161)
 if($edit-eq[IntPtr]::Zero){throw 'Missing command bar'}
 [WV]::SetText($edit,$command);if(![WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)){throw 'Cannot queue command'}
 Wait-WVQuiet $v
}
$results=[Collections.Generic.List[object]]::new();$esc=[string][char]27;$bs=[string][char]8
function Check($name,[scriptblock]$body){
 if($env:WINVI_TEST_FILTER -and $name-notmatch$env:WINVI_TEST_FILTER){return}
 $script:v=$null
 try{&$body;if($v-and(Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))};$results.Add(@{Test=$name;Result='PASS'});"PASS $name"}
 catch{$results.Add(@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{if($v-and!$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}}
}
function StartCase($name,[byte[]]$bytes){$script:path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path}
$ascii=[Text.Encoding]::ASCII;$original=$ascii.GetBytes("abc`ndef`nghi`n")
foreach($variant in 'implicit','explicit','relative','case','dotpath','hardlink'){
 Check "partial-denied-$variant" {
  StartCase "partial-denied-$variant" $original
  $target=switch($variant){
   'implicit' {''}
   'explicit' {' '+$path}
   'relative' {Ex $v (':cd '+$cases);' '+[IO.Path]::GetFileName($path)}
   'case' {' '+$path.ToUpperInvariant()}
   'dotpath' {$sub=Join-Path $cases 'sub';[void][IO.Directory]::CreateDirectory($sub);' '+(Join-Path $sub ('../'+[IO.Path]::GetFileName($path)))}
   'hardlink' {$alias=Join-Path $cases 'write-alias.txt';if(Test-Path $alias){Remove-Item -LiteralPath $alias};New-Item -ItemType HardLink -Path $alias -Target $path|Out-Null;' '+$alias}
  }
  Ex $v (':1write'+$target);Equal-Bytes $path $original
  if((Dialog $v)-ne[IntPtr]::Zero){throw 'Partial-current write should reject without an overwrite dialog'}
 }
}
Check 'partial-denied-buffer-remains-modified' {
 StartCase 'partial-denied-buffer-remains-modified' $original;Chars $v ('iX'+$esc);Ex $v ':2w';Equal-Bytes $path $original
 Ex $v ':%w';Equal-Bytes $path $ascii.GetBytes("Xabc`ndef`nghi`n")
}
foreach($command in ':1w!',':2w!',':2,3w!'){
 Check ('partial-forced-'+$command.Replace(':','').Replace(',','-').Replace('!','')) {
  StartCase ('forced-'+[guid]::NewGuid().ToString('N')) $original;Ex $v $command
  $want=switch($command){':1w!' {"abc`n"} ':2w!' {"def`n"} default {"def`nghi`n"}}
  Equal-Bytes $path $ascii.GetBytes($want)
 }
}
foreach($command in ':w',':%w',':1,3w'){
 Check ('whole-file-'+$command.Replace(':','').Replace(',','-').Replace('%','all')) {
  StartCase ('whole-'+[guid]::NewGuid().ToString('N')) $original;Chars $v ('iX'+$esc);Ex $v $command;Equal-Bytes $path $ascii.GetBytes("Xabc`ndef`nghi`n")
 }
}
Check 'partial-new-file-allowed' {
 StartCase 'partial-new-file-allowed' $original;$target=Join-Path $cases 'new-range.txt';Ex $v (':2w '+$target);Equal-Bytes $target $ascii.GetBytes("def`n");Equal-Bytes $path $original
}
Check 'partial-append-current-allowed' {
 StartCase 'partial-append-current-allowed' $original;Ex $v (':1w >> '+$path);Equal-Bytes $path ($original+$ascii.GetBytes("abc`n"))
}
foreach($command in ':3w!',':2,3w!',':%w'){
 Check ('incomplete-final-'+$command.Replace(':','').Replace(',','-').Replace('!','').Replace('%','all')) {
  StartCase ('incomplete-'+[guid]::NewGuid().ToString('N')) $ascii.GetBytes("abc`ndef`nghi")
  if($command-eq':%w'){Chars $v ('iX'+$esc)};Ex $v $command
  $want=switch($command){':3w!' {'ghi'} ':2,3w!' {"def`nghi"} default {"Xabc`ndef`nghi"}}
  Equal-Bytes $path $ascii.GetBytes($want)
 }
}
Check 'incomplete-final-partial-denied' {
 $source=$ascii.GetBytes("abc`ndef`nghi");StartCase 'incomplete-final-partial-denied' $source;Ex $v ':3w';Equal-Bytes $path $source
}
function ReplaceCase($name,$text,$keys,$expected,[Text.Encoding]$enc=$ascii,[scriptblock]$setup=$null,[bool]$undo=$true){
 Check $name {
  $initial=$enc.GetPreamble()+$enc.GetBytes($text);StartCase $name $initial;if($setup){&$setup $v}
  Chars $v $keys;Wait-WVQuiet $v;Save-WV $v;$want=$enc.GetPreamble()+$enc.GetBytes($expected);Equal-Bytes $path $want
  if($undo){Ex $v ':undo';Save-WV $v;Equal-Bytes $path $initial;Ex $v ':undo';Save-WV $v;Equal-Bytes $path $want}
 }
}
foreach($cp in 932,950){
 Check "$cp-windows-undo-restoration" {
  $enc=[Text.Encoding]::GetEncoding($cp);$wide=if($cp-eq932){[string][char]0x65e5}else{[string][char]0x4e2d}
  StartCase "$cp-windows-undo-restoration" $enc.GetBytes($wide+"bc`n");Ex $v (':set ansicodepage='+$cp);Menu $v 152
  Chars $v ('RX'+$bs+[char]26+$esc);Save-WV $v;Equal-Bytes $path $ascii.GetBytes("Xbc`n")
 }
}
Check 'utf8-windows-undo-restoration' {
 $enc=[Text.UTF8Encoding]::new($true);$wide=[char]::ConvertFromUtf32(0x1f600)
 StartCase 'utf8-windows-undo-restoration' ($enc.GetPreamble()+$enc.GetBytes($wide+"bc`n"))
 Chars $v ('RX'+$bs+[char]26+$esc);Save-WV $v;Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes("Xbc`n"))
}
ReplaceCase 'replace-empty-file' '' ('RZ'+$bs+$esc) ''
ReplaceCase 'replace-no-final-newline' 'abc' ('$RX'+$bs+$esc) 'abc'
ReplaceCase 'replace-crlf-restore' "abc`r`ndef`r`n" ('RX'+$bs+$esc) "abc`r`ndef`r`n"
ReplaceCase 'replace-count-repeat' "abcdef`n" ('2RXY'+$bs+$esc) "XXcdef`n"
ReplaceCase 'replace-one-restore' "abc`n" ('RX'+$bs+$esc) "abc`n"
ReplaceCase 'replace-partial-restore' "abc`n" ('RXY'+$bs+$esc) "Xbc`n"
ReplaceCase 'replace-two-restore' "abc`n" ('RXY'+$bs+$bs+$esc) "abc`n"
ReplaceCase 'replace-restore-retype' "abc`n" ('RX'+$bs+'Z'+$esc) "Zbc`n"
ReplaceCase 'replace-append-delete' "abc`n" ('$R1234'+$bs+$bs+$bs+$esc) "ab1`n"
ReplaceCase 'replace-append-restore-all' "abc`n" ('$R1234'+($bs*4)+$esc) "abc`n"
ReplaceCase 'replace-backspace-before-start' "abc`n" ('R'+$bs+$esc) "abc`n" $ascii $null $false
ReplaceCase 'replace-backspace-before-run' "abc`n" ('lR'+$bs+$esc) "abc`n" $ascii $null $false
ReplaceCase 'replace-new-session-no-old-restoration' "abc`n" ('RX'+$esc+'lR'+$bs+$esc) "Xbc`n"
ReplaceCase 'insert-backspace-unchanged' "abc`n" ('iX'+$bs+$esc) "abc`n"
ReplaceCase 'single-replacement-unchanged' "abc`n" 'rX' "Xbc`n"
Check 'replace-restore-after-save' {
 StartCase 'replace-restore-after-save' $original;Chars $v 'RX';Save-WV $v;Equal-Bytes $path $ascii.GetBytes("Xbc`ndef`nghi`n")
 Chars $v ($bs+$esc);Save-WV $v;Equal-Bytes $path $original
}
Check 'replace-arrow-ends-restoration-run' {
 StartCase 'replace-arrow-ends-restoration-run' $original;Chars $v 'RX';[void][WV]::Send($v.H,0x100,0x25,0);[void][WV]::Send($v.H,0x100,0x27,0)
 Chars $v ($bs+$esc);Save-WV $v;Equal-Bytes $path $ascii.GetBytes("Xbc`ndef`nghi`n")
}
$encodings=@{utf8=[Text.UTF8Encoding]::new($true);utf16le=[Text.UnicodeEncoding]::new($false,$true);utf16be=[Text.UnicodeEncoding]::new($true,$true)}
foreach($name in $encodings.Keys){
 $enc=$encodings[$name];$wide=[string][char]0x4e2d;$emoji=[char]::ConvertFromUtf32(0x1f600)
 ReplaceCase "$name-restore-wide" ($wide+"bc`n") ('RX'+$bs+$esc) ($wide+"bc`n") $enc
 ReplaceCase "$name-restore-supplementary" ($emoji+"bc`n") ('RX'+$bs+$esc) ($emoji+"bc`n") $enc
 ReplaceCase "$name-erase-supplementary-replacement" "abc`n" ('R'+$emoji+$bs+$esc) "abc`n" $enc
 ReplaceCase "$name-width-change-twice" ($wide+"bc`n") ('RX'+$emoji+$bs+$bs+$esc) ($wide+"bc`n") $enc
}
foreach($cp in 932,950){foreach($mode in 151,152){
 $enc=[Text.Encoding]::GetEncoding($cp,[Text.EncoderFallback]::ExceptionFallback,[Text.DecoderFallback]::ExceptionFallback)
 $wide=if($cp-eq932){[string][char]0x65e5}else{[string][char]0x4e2d}
 $setup={param($v) Ex $v (':set ansicodepage='+$cp+' oemcodepage='+$cp);Menu $v $mode}
 ReplaceCase "$cp-$mode-restore-pair" ($wide+"bc`n") ('RX'+$bs+$esc) ($wide+"bc`n") $enc $setup
 ReplaceCase "$cp-$mode-erase-pair" "abc`n" ('R'+$wide+$bs+$esc) "abc`n" $enc $setup
 ReplaceCase "$cp-$mode-two-restores" ($wide+"bc`n") ('RX'+$wide+$bs+$bs+$esc) ($wide+"bc`n") $enc $setup
 ReplaceCase "$cp-$mode-page-boundary" (('a'*4095)+$wide+"X`n") ('4095lRX'+$bs+$esc) (('a'*4095)+$wide+"X`n") $enc $setup
}}
$results|ConvertTo-Json -Depth 4|Set-Content "$PSScriptRoot/write-replace-results.json"
if(@($results|Where-Object {$_.Result-eq'FAIL'}).Count){throw 'Write/replace regressions failed'}
"$($results.Count) write/replace application cases passed"
