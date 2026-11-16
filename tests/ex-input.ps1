. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function Begin-Input($v,$command){
 Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,$command);[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
}
function Wait-InputBar($v){
 $limit=(Get-Date).AddSeconds(15)
 do{
  $input=[WV]::GetDlgItem($v.H,161)
  if($input -ne [IntPtr]::Zero -and [WV]::Text($v.H) -match 'ex (append|insert|change) input'){return $input}
  Start-Sleep -Milliseconds 50
 }while((Get-Date) -lt $limit)
 throw 'Missing ex text input bar'
}
function InputCase($name,$source,$command,$entered,$expected,[Text.Encoding]$enc=$utf8,[bool]$cancel=$false,[bool]$undo=$true,$after=@(),$setup=@(),[int]$interrupt=27){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null;$phase='open'
 try{
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$source,$enc);$original=[IO.File]::ReadAllBytes($path);$v=Start-WV $path
  foreach($cmd in $setup){Ex $v $cmd;Wait-WVQuiet $v}
  Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,$command);[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  $phase='command-bar input';$input=Wait-InputBar $v
  # Normalize supplied fixtures into accepted Enter-terminated lines.
  $normalized=$entered.Replace("`r`n","`n").Replace("`r","`n")
  $lines=@(if($normalized.Length){$normalized.Split("`n")}else{@()})
  if($lines.Count -and $lines[-1] -eq '' -and $normalized.EndsWith("`n")){$lines=if($lines.Count -eq 2){@($lines[0])}else{$lines[0..($lines.Count-2)]}}
  foreach($line in $lines){[WV]::SetText($input,$line);[void][WV]::Send($input,0x102,13,0);if([WV]::GetDlgItem($v.H,161) -eq [IntPtr]::Zero){break}}
  if([WV]::GetDlgItem($v.H,161) -ne [IntPtr]::Zero){
   if($cancel){[WV]::SetText($input,'unfinished');[void][WV]::Send($input,$(if($interrupt -lt 0){0x100}else{0x102}),[math]::Abs($interrupt),0)}
   else{[WV]::SetText($input,'.');[void][WV]::Send($input,0x102,13,0)}
  }
  Wait-WVQuiet $v;if([WV]::GetDlgItem($v.H,161) -ne [IntPtr]::Zero){throw 'Input bar remained open'}
  foreach($cmd in $after){if($cmd.StartsWith(':')){Ex $v $cmd}else{Menu $v 119;Chars $v $cmd};Wait-WVQuiet $v}
  $phase='save';Save-WV $v;$want=$enc.GetPreamble()+$enc.GetBytes($expected);Equal-Bytes $path $want
  if($undo){$phase='undo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original;$phase='redo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path $want}
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL $name ($phase): $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
InputCase 'append-default' "a`nb`n" ':append' 'X' "a`nX`nb`n"
InputCase 'append-zero' "a`nb`n" ':0a' "X`r`nY" "X`nY`na`nb`n"
InputCase 'append-end' "a`nb`n" ':$append' "X`nY`n" "a`nb`nX`nY`n"
InputCase 'insert-default' "a`nb`n" ':insert' 'X' "X`na`nb`n"
InputCase 'insert-address' "a`nb`n" ':2i' "X`rY`n" "a`nX`nY`nb`n"
InputCase 'change-default' "a`nb`n" ':change' 'X' "X`nb`n"
InputCase 'change-range' "a`nb`nc`nd`n" ':2,3c' "X`nY" "a`nX`nY`nd`n"
InputCase 'change-percent' "a`nb`n" ':%change' 'X' "X`n"
InputCase 'change-count' "a`nb`nc`nd`n" ':2c 2' 'X' "a`nX`nd`n"
InputCase 'change-range-count' "a`nb`nc`nd`n" ':1,2c 2' 'X' "a`nX`nd`n"
InputCase 'change-empty-deletes' "a`nb`nc`n" ':2c' '' "a`nc`n"
InputCase 'change-delete-all' "a`nb`n" ':%c' '' ''
InputCase 'append-empty-noop' "a`nb`n" ':append' '' "a`nb`n" $utf8 $false $false
InputCase 'insert-empty-noop' "a`nb`n" ':insert' '' "a`nb`n" $utf8 $false $false
InputCase 'append-cancel' "a`nb`n" ':append' 'X' "a`nX`nb`n" $utf8 $true
InputCase 'change-cancel' "a`nb`n" ':%c' 'X' "X`n" $utf8 $true
InputCase 'dot-terminates' "a`nb`n" ':1a' "X`r`n.`r`nignored" "a`nX`nb`n"
InputCase 'whitespace-and-blank-lines' "a`nb`n" ':1a' " .`r`n..`r`n`t  X`r`n`r`n" "a`n .`n..`n`t  X`n`nb`n"
InputCase 'append-incomplete-eof' "a`nb" ':$a' 'X' "a`nb`nX`n"
InputCase 'insert-before-incomplete-eof' "a`nb" ':2i' 'X' "a`nX`nb"
InputCase 'change-incomplete-eof' "a`nb" ':2c' 'X' "a`nX`n"
InputCase 'change-empty-cr-lf-seam' "a`rb`n`nc`n" ':2c' '' "a`r`n`nc`n"
InputCase 'append-empty-line-after-cr' "a`rb`n`nc`n" ':1a' "`n" "a`r`n`nb`n`nc`n"
InputCase 'change-cr-before-empty-lf' "a`rb`rc`rd`r`n`nz`r" ':5c' 'X' "a`rb`rc`rd`r`nX`r`nz`r"
InputCase 'insert-cr-before-empty-lf' "a`rb`rc`rd`r`n`nz`r" ':5i' 'X' "a`rb`rc`rd`r`nX`r`n`nz`r"
InputCase 'append-empty-buffer' '' ':append' 'X' "X`r`n"
InputCase 'insert-empty-buffer' '' ':insert' 'X' "X`r`n"
InputCase 'change-empty-buffer' '' ':change' 'X' "X`r`n"
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 InputCase ('append-'+$enc[0]) "é`r`n中`r`n" ':1a' '😀' "é`r`n😀`r`n中`r`n" $enc[1]
 InputCase ('change-'+$enc[0]) "é`r`n中`r`n" ':%c' "😀`nX" "😀`r`nX`r`n" $enc[1]
 InputCase ('empty-bom-'+$enc[0]) '' ':insert' '中' "中`r`n" $enc[1]
}
InputCase 'change-page-boundary' (('a'*8191)+"`nb`nc`n") ':1,2c' "X`nY" "X`nY`nc`n"
InputCase 'append-large-input' "a`nb`n" ':1a' ('X'*131071) ("a`n"+('X'*131071)+"`nb`n")
InputCase 'change-large-input' (('a'*131071)+"`nb`n") ':1c' ('X'*131071) (('X'*131071)+"`nb`n")
InputCase 'append-cursor-last-line' "a`nb`n" ':1a' "X`nY" "a`nX`nZ`nb`n" $utf8 $false $false @('rZ')
InputCase 'change-cursor-last-line' "a`nb`nc`n" ':1,2c' "X`nY" "X`nZ`nc`n" $utf8 $false $false @('rZ')
InputCase 'change-preserves-register' "a`nb`n" ':1c' 'X' "b`nX`nb`n" $utf8 $false $false @(':0put z') @(':2yank z')
InputCase 'change-tracks-surviving-mark' "a`nb`nc`n" ':1c' 'X' "X`nb`nZ`n" $utf8 $false $false @('`a','rZ') @(':3mark a')
InputCase 'change-cr-line-count' "a`rb`rc`rd`r`n`nz`r" ':5c' 'X' "z`ra`rb`rc`rd`r`nX`r`nz`r" $utf8 $false $false @(':$copy0')
InputCase 'append-cr-lf-line-count' "a`rb`n`nc`n" ':1a' "`n" "c`na`r`n`nb`n`nc`n" $utf8 $false $false @(':$copy0')
InputCase 'append-ctrl-c' "a`nb`n" ':1a' "X`nY" "a`nX`nY`nb`n" $utf8 $true $true @() @() 3
InputCase 'change-ctrl-break' "a`nb`n" ':%c' "X`nY" "X`nY`n" $utf8 $true $true @() @() -3
InputCase 'change-interrupt-no-lines' "a`nb`n" ':1c' '' "b`n" $utf8 $true
InputCase 'append-interrupt-no-lines' "a`nb`n" ':1a' '' "a`nb`n" $utf8 $true $false
InputCase 'insert-interrupt-accepted' "a`nb`n" ':2i' "X`nY" "a`nX`nY`nb`n" $utf8 $true
if(!$env:WINVI_TEST_FILTER -or 'unrepresentable-input-retained' -match $env:WINVI_TEST_FILTER){
 $v=$null
 try{
  $path=Join-Path $cases 'unrepresentable.txt';$original=$utf8.GetBytes("a`nb`n");[IO.File]::WriteAllBytes($path,$original);$v=Start-WV $path;Ex $v ':set ansimode ansicp=1252'
  Begin-Input $v ':1change';$input=Wait-InputBar $v
  [WV]::SetText($input,'X');[void][WV]::Send($v.H,2258,0,0)
  [WV]::SetText($input,'中');[void][WV]::Send($v.H,2258,0,0)
  if([WV]::Text($v.H) -notmatch 'cannot be encoded'){throw 'Missing encoding explanation'}
  $buffer=[Runtime.InteropServices.Marshal]::AllocHGlobal(16)
  try{[void][WV]::Send($input,13,8,$buffer.ToInt64());$retained=[Runtime.InteropServices.Marshal]::PtrToStringUni($buffer)}finally{[Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)}
  if($retained -cne '中'){throw 'Invalid line was not retained'}
  [void][WV]::Send($input,0x102,3,0);Wait-WVQuiet $v;Save-WV $v;Equal-Bytes $path $utf8.GetBytes("X`nb`n")
  Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test='unrepresentable-input-retained';Result='PASS'});'PASS unrepresentable-input-retained'
 }catch{$results.Add([pscustomobject]@{Test='unrepresentable-input-retained';Result='FAIL';Detail=$_.Exception.Message});"FAIL unrepresentable-input-retained: $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
foreach($mode in 'validation','close','backspace','tab'){
 $name='input-'+$mode
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){continue}
 $v=$null
 try{
  $path=Join-Path $cases ($name+'.txt');$original=$utf8.GetBytes("a`nb`n");[IO.File]::WriteAllBytes($path,$original);$v=Start-WV $path
  Begin-Input $v ':1a';$input=Wait-InputBar $v
  [WV]::SetText($input,'X');[void][WV]::Send($input,0x102,13,0)
  if($mode -eq 'validation'){
   [WV]::SetText($input,('Z'*1000000))
   [void][WV]::PostMessageW($input,0x102,[UIntPtr]13,[IntPtr]::Zero)
   [void][WV]::PostMessageW($input,0x102,[UIntPtr]27,[IntPtr]::Zero)
  }elseif($mode -eq 'close'){
   [WV]::SetText($input,'unfinished');[void][WV]::Send($v.H,16,0,0)
  }elseif($mode -eq 'tab'){
   [WV]::SetText($input,'Y');[void][WV]::Send($input,0xb1,1,1);[void][WV]::Send($input,0x102,9,0);[void][WV]::Send($input,0x102,90,0)
   [void][WV]::Send($input,0x102,13,0);[WV]::SetText($input,'.');[void][WV]::Send($input,0x102,13,0)
  }else{
   [WV]::SetText($input,'YQ');[void][WV]::Send($input,0xb1,2,2);[void][WV]::Send($input,0x102,8,0)
   [void][WV]::Send($input,0x102,13,0);[WV]::SetText($input,'.');[void][WV]::Send($input,0x102,13,0)
  }
  Wait-WVQuiet $v;if([WV]::GetDlgItem($v.H,161) -ne [IntPtr]::Zero){throw 'Input bar remained open'}
  $want=$utf8.GetBytes($(if($mode -eq 'backspace'){"a`nX`nY`nb`n"}elseif($mode -eq 'tab'){"a`nX`nY`tZ`nb`n"}else{"a`nX`nb`n"}))
  Save-WV $v;Equal-Bytes $path $want;Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original;Ex $v ':undo';Save-WV $v;Equal-Bytes $path $want
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{Stop-WV $v}
}

# Visible windows deliver edit notifications that hidden-window tests miss.
$savedVisible=$env:WINVI_TEST_VISIBLE
try{
 $env:WINVI_TEST_VISIBLE='1'
 InputCase 'append-visible-startup' "a`nb`n" ':append' 'X' "a`nX`nb`n"
 InputCase 'insert-visible-startup' "a`nb`n" ':insert' 'X' "X`na`nb`n"
 InputCase 'change-visible-startup' "a`nb`n" ':change' 'X' "X`nb`n"
}finally{$env:WINVI_TEST_VISIBLE=$savedVisible}

$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-input-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Ex text-input application regressions failed'}
"$($results.Count) text-input application cases passed"
