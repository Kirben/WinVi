. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function InputCase($name,$source,$command,$entered,$expected,[Text.Encoding]$enc=$utf8,[bool]$cancel=$false,[bool]$undo=$true,$after=@(),$setup=@()){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null;$phase='open'
 try{
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$source,$enc);$original=[IO.File]::ReadAllBytes($path);$v=Start-WV $path
  foreach($cmd in $setup){Ex $v $cmd;Wait-WVQuiet $v}
  Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,$command);[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  $phase='input dialog';$limit=(Get-Date).AddSeconds(15)
  do{
   $dlg=Dialog $v;$input=if($dlg -ne [IntPtr]::Zero){[WV]::GetDlgItem($dlg,32001)}else{[IntPtr]::Zero}
   if($input -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 50}
  }while($input -eq [IntPtr]::Zero -and (Get-Date) -lt $limit)
  if($input -eq [IntPtr]::Zero){throw ('Missing line input; dialog='+[WV]::Text($dlg))}
  [WV]::SetText($input,$entered)
  [void][WV]::Send($dlg,0x111,$(if($cancel){2}else{1}),0)
  Wait-WVQuiet $v;if((Dialog $v) -ne [IntPtr]::Zero){throw 'Line dialog remained open'}
  foreach($cmd in $after){if($cmd.StartsWith(':')){Ex $v $cmd}else{Menu $v 119;Chars $v $cmd};Wait-WVQuiet $v}
  $phase='save';Save-WV $v;$want=$enc.GetPreamble()+$enc.GetBytes($expected);Equal-Bytes $path $want
  if($undo -and !$cancel){$phase='undo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path $original;$phase='redo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path $want}
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
InputCase 'append-cancel' "a`nb`n" ':append' 'X' "a`nb`n" $utf8 $true
InputCase 'change-cancel' "a`nb`n" ':%c' 'X' "a`nb`n" $utf8 $true
InputCase 'literal-dot-and-whitespace' "a`nb`n" ':1a' ".`r`n`t  X`r`n`r`n" "a`n.`n`t  X`n`nb`n"
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
if(!$env:WINVI_TEST_FILTER -or 'unrepresentable-input-retained' -match $env:WINVI_TEST_FILTER){
 $v=$null
 try{
  $path=Join-Path $cases 'unrepresentable.txt';$original=$utf8.GetBytes("a`nb`n");[IO.File]::WriteAllBytes($path,$original);$v=Start-WV $path;Ex $v ':set ansimode ansicp=1252'
  Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,':1change');[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  $limit=(Get-Date).AddSeconds(10);do{$dlg=Dialog $v;$input=if($dlg -ne [IntPtr]::Zero){[WV]::GetDlgItem($dlg,32001)}else{[IntPtr]::Zero};if($input -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 50}}while($input -eq [IntPtr]::Zero -and (Get-Date) -lt $limit)
  if($input -eq [IntPtr]::Zero){throw 'Missing input dialog'}
  [WV]::SetText($input,'中');[void][WV]::Send($dlg,0x111,1,0)
  $buffer=[Runtime.InteropServices.Marshal]::AllocHGlobal(16)
  try{[void][WV]::Send($input,13,8,$buffer.ToInt64());$retained=[Runtime.InteropServices.Marshal]::PtrToStringUni($buffer)}finally{[Runtime.InteropServices.Marshal]::FreeHGlobal($buffer)}
  if($retained -cne '中' -or [WV]::Text([WV]::GetDlgItem($dlg,32002)) -notmatch 'cannot be represented'){throw 'Unrepresentable text was not retained with an explanation'}
  [void][WV]::Send($dlg,0x111,2,0);Wait-WVQuiet $v;Save-WV $v;Equal-Bytes $path $original
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test='unrepresentable-input-retained';Result='PASS'});'PASS unrepresentable-input-retained'
 }catch{$results.Add([pscustomobject]@{Test='unrepresentable-input-retained';Result='FAIL';Detail=$_.Exception.Message});"FAIL unrepresentable-input-retained: $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
if(!$env:WINVI_TEST_FILTER -or 'cancel-during-input-validation' -match $env:WINVI_TEST_FILTER){
 $v=$null
 try{
  $path=Join-Path $cases 'cancel-validation.txt';$original=$utf8.GetBytes("a`nb`n");[IO.File]::WriteAllBytes($path,$original);$v=Start-WV $path
  Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,':1change');[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  $limit=(Get-Date).AddSeconds(10);do{$dlg=Dialog $v;$input=if($dlg -ne [IntPtr]::Zero){[WV]::GetDlgItem($dlg,32001)}else{[IntPtr]::Zero};if($input -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 50}}while($input -eq [IntPtr]::Zero -and (Get-Date) -lt $limit)
  if($input -eq [IntPtr]::Zero){throw 'Missing input dialog'}
  [WV]::SetText($input,('X'*1000000));[void][WV]::PostMessageW($dlg,0x111,[UIntPtr]1,[IntPtr]::Zero);[void][WV]::PostMessageW($dlg,0x111,[UIntPtr]2,[IntPtr]::Zero)
  Wait-WVQuiet $v;if((Dialog $v) -ne [IntPtr]::Zero){throw 'Validation cancellation did not close the dialog'}
  Save-WV $v;Equal-Bytes $path $original;if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test='cancel-during-input-validation';Result='PASS'});'PASS cancel-during-input-validation'
 }catch{$results.Add([pscustomobject]@{Test='cancel-during-input-validation';Result='FAIL';Detail=$_.Exception.Message});"FAIL cancel-during-input-validation: $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-input-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Ex text-input application regressions failed'}
"$($results.Count) text-input application cases passed"
