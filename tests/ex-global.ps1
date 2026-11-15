. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($false)
function Check($name,[scriptblock]$action){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $script:v=$null;$watch=[Diagnostics.Stopwatch]::StartNew()
 try{& $action;if($script:v -and (Get-Item $script:v.Log).Length){throw ([IO.File]::ReadAllText($script:v.Log))};$results.Add([pscustomobject]@{Test=$name;Result='PASS';Seconds=$watch.Elapsed.TotalSeconds});"PASS $name"}
 catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=($script:phase+': '+$_.Exception.Message)});"FAIL ${name} ($script:phase): $($_.Exception.Message)"}
 finally{Stop-WV $script:v}
}
function Edit-Case($name,$inputText,$command,$expected,[Text.Encoding]$encoding=$utf8,[bool]$undo=$true){
 Check $name {
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$inputText,$encoding)
  $original=[IO.File]::ReadAllBytes($path);$script:v=Start-WV $path
  Ex $v ':set magic';[WV]::MessageTimeout=120000;$script:phase='command';Ex $v $command;Wait-WVQuiet $v 120;Save-WV $v
  $want=$encoding.GetPreamble()+$encoding.GetBytes($expected);Equal-Bytes $path $want
  if($undo){$script:phase='undo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $original;$script:phase='redo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $want}
 }
}
Edit-Case 'g-delete' "hit`r`nkeep`r`nhit`r`n" ':g/hit/d' "keep`r`n"
Edit-Case 'v-delete' "hit`nkeep`nhit`n" ':v/hit/d' "hit`nhit`n"
Edit-Case 'g-bang-inverse' "hit`nkeep`nhit`n" ':g!/hit/d' "hit`nhit`n"
Edit-Case 'global-long-name' "hit`nkeep`nhit`n" ':global/hit/delete' "keep`n"
Edit-Case 'addressed-range' "hit`nhit`nhit`nhit`n" ':2,3g/hit/d' "hit`nhit`n"
Edit-Case 'percent-range' "hit`nkeep`n" ':%g/hit/d' "keep`n"
Edit-Case 'single-address' "hit`nhit`nhit`n" ':2g/hit/d' "hit`nhit`n"
Edit-Case 'unterminated-last' "keep`nhit" ':g/hit/d' "keep`n"
Edit-Case 'delete-all' "hit`nhit`n" ':g/hit/d' ''
Edit-Case 'empty-lines' "a`n`n`nb`n" ':g/^$/d' "a`nb`n"
Edit-Case 'substitution' "hit one`nkeep`nhit two`n" ':g/hit/s/hit/changed/' "changed one`nkeep`nchanged two`n"
Edit-Case 'inverse-substitution' "hit`nkeep`nother`n" ':v/hit/s/^/X/' "hit`nXkeep`nXother`n"
Edit-Case 'substitute-global-flag' "hit hit`nkeep`nhit hit`n" ':g/hit/s/hit/X/g' "X X`nkeep`nX X`n"
Edit-Case 'no-reselect-new-matches' "a`na`n" ':g/a/s/a/aa/g' "aa`naa`n"
Edit-Case 'reuse-global-pattern' "hit`nkeep`nhit`n" ':g/hit/s//X/' "X`nkeep`nX`n"
Edit-Case 'insert-newlines-without-reselection' "hit`nhit`n" ':g/hit/s/hit/hit\%0ahit/' "hit`nhit`nhit`nhit`n"
Edit-Case 'consume-selected-original-line' "hit`nhit`nhit`n" ':g/hit/s/hit\%0ahit/X/' "X`nhit`n"
Edit-Case 'alternate-delimiter' "a/b`nkeep`na/b`n" ':g#a/b#d' "keep`n"
Edit-Case 'escaped-delimiter' "a/b`nkeep`na/b`n" ':g/a\/b/d' "keep`n"
Edit-Case 'delimiter-in-character-class' "/`nkeep`na`n" ':g/[a/]/d' "keep`n"
Edit-Case 'trailing-command-space' "hit`nkeep`n" ':g/hit/d   ' "keep`n"
Check 'reuse-previous-search-pattern' {
 $path=Join-Path $cases 'reuse-previous.txt';[IO.File]::WriteAllText($path,"hit`nkeep`nhit`n",$utf8);$script:v=Start-WV $path
 Ex $v '/hit/';Ex $v ':g//d';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("keep`n")
}
foreach($mode in 'viewonly','hexmode'){
 Check ($mode+'-rejects-global-edit') {
  $path=Join-Path $cases ($mode+'.txt');[IO.File]::WriteAllText($path,"hit`nkeep`n",$utf8);$script:v=Start-WV $path
  Ex $v (':set '+$mode);Ex $v ':g/hit/d';Ex $v (':set no'+$mode);Save-WV $v;Equal-Bytes $path $utf8.GetBytes("hit`nkeep`n")
 }
}
foreach($enc in @(@('utf8bom',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Edit-Case ($enc[0]+'-delete') "hit`r`n中-é`r`nhit`r`n" ':g/hit/d' "中-é`r`n" $enc[1]
 Edit-Case ($enc[0]+'-substitute') "中hit`nhit中`n" ':g/hit/s/hit/X/' "中X`nX中`n" $enc[1]
 Edit-Case ($enc[0]+'-start-anchor') "hit`nkeep`nhit`n" ':g/^hit/d' "keep`n" $enc[1]
}
Edit-Case 'page-boundary' (('a'*4094)+"`nhit`nkeep`nhit`n") ':g/hit/d' (('a'*4094)+"`nkeep`n")
Edit-Case 'many-original-lines' (("hit`nkeep`n")*2000) ':g/hit/d' (("keep`n")*2000)
foreach($cmd in ':g/no-match/d',':g/hit',':g/hit/g/keep/d',':g/hit/!echo unsafe',':g/hit/e other',':g/hit/d|d',':g/hit/1d',':g/hit/s/a/b/z',':3,1g/hit/d'){
 Edit-Case ('reject-'+[array]::IndexOf(@(':g/no-match/d',':g/hit',':g/hit/g/keep/d',':g/hit/!echo unsafe',':g/hit/e other',':g/hit/d|d',':g/hit/1d',':g/hit/s/a/b/z',':3,1g/hit/d'),$cmd)) "hit`nkeep`n" $cmd "hit`nkeep`n" $utf8 $false
}
foreach($spec in @(@('default-output',':g/hit/',"hit 1`r`nhit 2`r`n"),@('number-output',':g/hit/#',"1`thit 1`r`n3`thit 2`r`n"),@('line-output',':g/hit/=',"1`r`n3`r`n"))){
 Check $spec[0] {
  $path=Join-Path $cases ($spec[0]+'.txt');[IO.File]::WriteAllText($path,"hit 1`nkeep`nhit 2`n",$utf8);$script:v=Start-WV $path
  Ex $v $spec[1];$windows=[WV]::Windows($v.P.Id);$output=$windows | Where-Object {[WV]::Class($_) -eq 'WinViExOutput'} | Select-Object -First 1
  if(!$output){throw 'Missing output window'}
  $edit=[WV]::GetDlgItem($output,1);$buffer=[Runtime.InteropServices.Marshal]::AllocHGlobal(4096)
  try{[void][WV]::Send($edit,13,2048,$buffer.ToInt64());$text=[Runtime.InteropServices.Marshal]::PtrToStringUni($buffer)}finally{[Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)}
  if($text -ne $spec[2]){throw ('Unexpected output: '+$text)}
  [void][WV]::PostMessageW($output,16,[UIntPtr]::Zero,[IntPtr]::Zero)
 }
}
foreach($close in $false,$true){
 Check $(if($close){'close-during-global-cancels'}else{'cancel-pathological-global-pattern'}) {
  $path=Join-Path $cases ('cancel-global-'+$close+'.txt');$original=$utf8.GetBytes(('a'*34)+"c`n");[IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path
  Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161)
  [WV]::SetText($edit,(':g/'+('a*'*24)+'[b]/d'));[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 300
  if($close){[void][WV]::PostMessageW($v.H,16,[UIntPtr]::Zero,[IntPtr]::Zero)}else{[void][WV]::PostMessageW($v.H,0x102,[UIntPtr]3,[IntPtr]1)}
  Wait-WVQuiet $v 15;Save-WV $v;Equal-Bytes $path $original
  Ex $v ':1';Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v;Equal-Bytes $path ([byte[]](@(88)+$original))
 }
}
foreach($repeat in 1..3){Check ('cancel-during-execution-grouped-undo-redo-'+$repeat) {
 [WV]::MessageTimeout=30000
 $path=Join-Path $cases 'partial-global.txt';$original=$utf8.GetBytes(("hit`nkeep`n")*2000);[IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path
 $title=[WV]::Text($v.H);Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161);[WV]::SetText($edit,':g/hit/d')
 [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
 $limit=(Get-Date).AddSeconds(15);while([WV]::Text($v.H) -eq $title){if((Get-Date) -gt $limit){throw 'No first-edit title change'};Start-Sleep -Milliseconds 1}
 [void][WV]::PostMessageW($v.H,0x102,[UIntPtr]3,[IntPtr]1);Wait-WVQuiet $v 30;Save-WV $v
 $partial=[IO.File]::ReadAllBytes($path);if($partial.Length -ge $original.Length -or $partial.Length -le 10000){throw "Expected partial edits, got $($partial.Length) bytes"}
 $script:phase='partial undo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $original;$script:phase='partial redo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $partial
}}
foreach($acceptFirst in $false,$true){Check ('substitution-confirmation-cancel-stops-global-'+$acceptFirst) {
 [WV]::MessageTimeout=30000;$script:phase='confirm setup'
 $path=Join-Path $cases 'confirm-cancel.txt';$original=$utf8.GetBytes("hit`nhit`nhit`n");[IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path
 Menu $v 119;Chars $v ':';$edit=[WV]::GetDlgItem($v.H,161);[WV]::SetText($edit,':g/hit/s/hit/X/c')
 [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
 $limit=(Get-Date).AddSeconds(10);do{$dialog=Dialog $v;if((Get-Date) -gt $limit){throw 'Missing substitution confirmation'};Start-Sleep -Milliseconds 20}while(!$dialog)
 if($acceptFirst){[void][WV]::Send($dialog,0x111,1,0);Start-Sleep -Milliseconds 100;$limit=(Get-Date).AddSeconds(10);do{$dialog=Dialog $v;if((Get-Date) -gt $limit){throw 'Missing second confirmation'};Start-Sleep -Milliseconds 20}while(!$dialog)}
 $script:phase='confirm cancel';[void][WV]::Send($dialog,0x111,2,0);Start-Sleep -Milliseconds 150
 if((Dialog $v) -ne [IntPtr]::Zero){throw 'Cancel continued to another original line'}
 $script:phase='confirm save';Wait-WVQuiet $v 15;Save-WV $v;$expected=if($acceptFirst){$utf8.GetBytes("X`nhit`nhit`n")}else{$original};Equal-Bytes $path $expected
 if($acceptFirst){$script:phase='confirm undo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $original;$script:phase='confirm redo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $expected}
}}
$results|ConvertTo-Json -Depth 4|Set-Content "$PSScriptRoot/ex-global-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){exit 1}
"$($results.Count) global command cases passed"
