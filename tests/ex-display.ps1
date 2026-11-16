. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($false)
function Check($name,[scriptblock]$action){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $script:v=$null
 try{& $action;if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))};$results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"}
 catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{Stop-WV $script:v}
}
function Output($v,$cmd,$want){
 Ex $v $cmd;Wait-WVQuiet $v
 $windows=@([WV]::Windows($v.P.Id)|Where-Object {[WV]::Class($_) -eq 'WinViExOutput'})
 if($windows.Count -ne 1){throw "Expected one output window, found $($windows.Count)"}
 $window=$windows[0];$edit=[WV]::GetDlgItem($window,1)
 $length=[WV]::Send($edit,14,0,0);$buffer=[Runtime.InteropServices.Marshal]::AllocHGlobal(2*($length+1))
 try{[void][WV]::Send($edit,13,($length+1),$buffer.ToInt64());$actual=[Runtime.InteropServices.Marshal]::PtrToStringUni($buffer)}finally{[Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)}
 if($actual -cne $want){throw ("Output differs: actual=$($actual.Length), expected=$($want.Length); code units="+(($actual.ToCharArray()|ForEach-Object {'{0:X4}' -f [int]$_}) -join ' '))}
 [void][WV]::Send($window,16,0,0)
}
function DisplayCase($name,$text,$cmd,$want,[int]$cursor,[Text.Encoding]$enc=$utf8){
 Check $name {
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$text,$enc);$original=[IO.File]::ReadAllBytes($path);$script:v=Start-WV $path
  Output $v $cmd $want;Save-WV $v;Equal-Bytes $path $original
  # A subsequent copy verifies which line the display command made current.
  Ex $v ':copy 0';Save-WV $v
  $lines=$text -split '\r\n|\n|\r';$line=$lines[$cursor-1]
  $eol=if($text.Contains("`r`n")){"`r`n"}elseif($text.Contains("`r")){"`r"}else{"`n"}
  Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes($line+$eol+$text))
 }
}
DisplayCase 'print-current' "a`nb`nc`n" ':print' "a`r`n" 1
DisplayCase 'print-range' "a`nb`nc`n" ':2,3p' "b`r`nc`r`n" 3
DisplayCase 'print-percent' "a`nb`n" ':%p' "a`r`nb`r`n" 2
DisplayCase 'print-count' "a`nb`nc`nd`n" ':2p 2' "b`r`nc`r`n" 3
DisplayCase 'print-range-count' "a`nb`nc`nd`n" ':1,2p 2' "b`r`nc`r`n" 3
DisplayCase 'number-range' "a`nb`nc`n" ':2,3number' "2`tb`r`n3`tc`r`n" 3
DisplayCase 'number-alias' "a`nb`n" ':%#' "1`ta`r`n2`tb`r`n" 2
DisplayCase 'number-abbreviation' "a`nb`n" ':2nu' "2`tb`r`n" 2
DisplayCase 'list-controls' "a`t$\$([char]1)$([char]127)`n`nlast" ':1l' "a^I$\^A^?`$`r`n" 1
DisplayCase 'list-empty-line' "a`n`nlast" ':2list' "`$`r`n" 2
DisplayCase 'list-unterminated' "a`nlast" ':$l' "last`$`r`n" 2
DisplayCase 'print-tabs-unchanged' "a`tb`nc`n" ':p' "a`tb`r`n" 1
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 DisplayCase ('print-'+$enc[0]) "é`r`n中😀`r`n" ':%p' "é`r`n中😀`r`n" 2 $enc[1]
 DisplayCase ('list-'+$enc[0]) "é`t中😀`r`nlast" ':1l' "é^I中😀`$`r`n" 1 $enc[1]
}
DisplayCase 'print-page-boundary' (('a'*8191)+"`nb`n") ':1p' (('a'*8191)+"`r`n") 1
DisplayCase 'print-large-output' (('a'*131071)+"`nb`n") ':%p' (('a'*131071)+"`r`nb`r`n") 2
Check 'display-view-only' {
 $path=Join-Path $cases 'view-only.txt';[IO.File]::WriteAllText($path,"a`nb`n",$utf8);$script:v=Start-WV $path
 Ex $v ':set viewonly';Output $v ':%number' "1`ta`r`n2`tb`r`n";Ex $v ':set noviewonly';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("a`nb`n")
}
foreach($spec in @(@('join',':1,2join',"a b`nc`n"),@('copy',':1copy 3',"a`nb`nc`na`n"),@('move',':1move 3',"b`nc`na`n"),@('global',':g/a/s/a/A/',"A`nb`nc`n"))){
 Check ('undo-'+$spec[0]) {
  $path=Join-Path $cases ('undo-'+$spec[0]+'.txt');$original=$utf8.GetBytes("a`nb`nc`n");[IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path
  Ex $v $spec[1];Save-WV $v;Equal-Bytes $path $utf8.GetBytes($spec[2])
  Output $v ':1p' (($spec[2] -split "`n")[0]+"`r`n")
  Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original
  Ex $v ':u';Save-WV $v;Equal-Bytes $path $utf8.GetBytes($spec[2])
 }
}
foreach($enc in @(@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Check ('undo-'+$enc[0]) {
  $path=Join-Path $cases ('undo-'+$enc[0]+'.txt');$original=$enc[1].GetPreamble()+$enc[1].GetBytes("é`r`n中`r`n");[IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path
  Ex $v ':join';Save-WV $v;Equal-Bytes $path ($enc[1].GetPreamble()+$enc[1].GetBytes("é 中`r`n"))
  Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original
  Ex $v ':undo';Save-WV $v;Equal-Bytes $path ($enc[1].GetPreamble()+$enc[1].GetBytes("é 中`r`n"))
 }
}
Check 'print-ansi-codepage' {
 $path=Join-Path $cases 'ansi.txt';[IO.File]::WriteAllBytes($path,[byte[]]@(0x80,10));$script:v=Start-WV $path
 Ex $v ':set ansicp=1252';Output $v ':p' "€`r`n";Save-WV $v;Equal-Bytes $path ([byte[]]@(0x80,10))
}
Check 'display-preserves-register' {
 $path=Join-Path $cases 'register.txt';[IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);$script:v=Start-WV $path
 Ex $v ':2yank';Output $v ':3list' "c`$`r`n";Ex $v ':0put';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("b`na`nb`nc`n")
}
Check 'undo-large-operation' {
 $path=Join-Path $cases 'undo-large.txt';$text=('a'*131071)+"`nb`n";$original=$utf8.GetBytes($text);$changed=$utf8.GetBytes($text+('a'*131071)+"`n")
 [IO.File]::WriteAllBytes($path,$original);$script:v=Start-WV $path;Ex $v ':1copy $';Save-WV $v;Equal-Bytes $path $changed
 Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original;Ex $v ':undo';Save-WV $v;Equal-Bytes $path $changed
}

$ztext=((& {foreach($i in 1..10){"L$i"}}) -join "`n")+"`n"
DisplayCase 'z-adjacent-flags' "a`nb`n" ':1zlp' "a`$`r`nb`$`r`n" 2
DisplayCase 'z-address' $ztext ':3z 3' "L3`r`nL4`r`nL5`r`n" 5
DisplayCase 'z-bare' "a`nb`nc`n" ':z 2' "b`r`nc`r`n" 3
DisplayCase 'z-plus' $ztext ':4z+3' "L4`r`nL5`r`nL6`r`n" 6
DisplayCase 'z-minus' $ztext ':5z-3' "L3`r`nL4`r`nL5`r`n" 5
DisplayCase 'z-dot' $ztext ':5z.5' "L3`r`nL4`r`nL5`r`nL6`r`nL7`r`n" 7
DisplayCase 'z-caret' $ztext ':8z^3' "L3`r`nL4`r`nL5`r`n" 5
DisplayCase 'z-caret-top' $ztext ':2z^3' "L1`r`nL2`r`nL3`r`n" 3
DisplayCase 'z-equal' $ztext ':5z=5' ((('-'*40)+"`r`nL5`r`n"+('-'*40)+"`r`n").Insert(0,"L4`r`n")+"L6`r`n") 5
DisplayCase 'z-number-list' "a`t$([char]1)`nb`n" ':1z+2#l' "1`ta^I^A`$`r`n2`tb`$`r`n" 2
DisplayCase 'z-huge-count' $ztext ':8z+9223372036854775807' "L8`r`nL9`r`nL10`r`n" 10
DisplayCase 'z-top-clip' $ztext ':1z.5' "L1`r`nL2`r`nL3`r`n" 3
DisplayCase 'z-bottom-clip' $ztext ':10z.5' "L8`r`nL9`r`nL10`r`n" 10
DisplayCase 'z-utf16' "é`r`n中😀`r`n" ':1z+2' "é`r`n中😀`r`n" 2 ([Text.UnicodeEncoding]::new($false,$true))
DisplayCase 'z-large-output' (('a'*131071)+"`nb`n") ':1z+2' (('a'*131071)+"`r`nb`r`n") 2
Check 'z-preserves-undo-register' {
 $path=Join-Path $cases 'z-undo.txt';[IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8);$script:v=Start-WV $path
 Ex $v ':2yank';Ex $v ':1,2join';Output $v ':1z+2' "a b`r`nc`r`n"
 Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
 Ex $v ':0put';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("b`na`nb`nc`n")
}

$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-display-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Ex display/undo regressions failed'}
"$($results.Count) display/undo application cases passed"
