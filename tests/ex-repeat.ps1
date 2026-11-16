. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function Case($name,$text,$setup,$command,$before,$after,[Text.Encoding]$enc=$utf8){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null;$phase='open'
 try{
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$text,$enc);$v=Start-WV $path
  Ex $v ':set magic';foreach($cmd in $setup){Ex $v $cmd;Wait-WVQuiet $v}
  Save-WV $v;Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes($before))
  $phase=$command;Ex $v $command;Wait-WVQuiet $v;Save-WV $v;Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes($after))
  $phase='undo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes($before))
  $phase='redo';Ex $v ':undo';Save-WV $v;Equal-Bytes $path ($enc.GetPreamble()+$enc.GetBytes($after))
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL $name ($phase): $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
Case 'amp-previous-substitution' "foo bar`nfoo bar`n" @(':1s/foo/X/','/bar/') ':2&' "X bar`nfoo bar`n" "X bar`nX bar`n"
Case 'tilde-latest-search' "foo bar`nfoo bar`n" @(':1s/foo/X/','/bar/') ':2~' "X bar`nfoo bar`n" "X bar`nfoo X`n"
Case 'amp-default-current-line' "foo`nfoo`n" @(':1s/foo/X/',':2') ':&' "X`nfoo`n" "X`nX`n"
Case 'amp-explicit-range' "foo`nfoo`nfoo`nfoo`n" @(':1s/foo/X/') ':2,4&' "X`nfoo`nfoo`nfoo`n" "X`nX`nX`nX`n"
Case 'amp-percent' "foo foo`nfoo foo`n" @(':1s/foo/X/') ':%&' "X foo`nfoo foo`n" "X X`nX foo`n"
Case 'amp-global-flag' "foo foo`nfoo foo`n" @(':1s/foo/X/g') ':2&g' "X X`nfoo foo`n" "X X`nX X`n"
Case 'amp-flags-reset' "foo foo`nfoo foo`n" @(':1s/foo/X/g') ':2&' "X X`nfoo foo`n" "X X`nX foo`n"
Case 'amp-count' "foo`nfoo`nfoo`nfoo`n" @(':1s/foo/X/') ':2& 2' "X`nfoo`nfoo`nfoo`n" "X`nX`nX`nfoo`n"
Case 'amp-range-count' "foo`nfoo`nfoo`nfoo`n" @(':1s/foo/X/') ':1,2&2' "X`nfoo`nfoo`nfoo`n" "X`nX`nX`nfoo`n"
Case 'amp-count-clips-eof' "foo`nfoo`n" @(':1s/foo/X/') ':2& 99' "X`nfoo`n" "X`nX`n"
Case 'tilde-global-count' "foo bar bar`nfoo bar bar`nfoo bar bar`n" @(':1s/foo/X/','/bar/') ':2~g 2' "X bar bar`nfoo bar bar`nfoo bar bar`n" "X bar bar`nfoo X X`nfoo X X`n"
Case 'tilde-becomes-last-substitution' "foo bar`nfoo bar`nfoo bar`n" @(':1s/foo/X/','/bar/',':2~') ':3&' "X bar`nfoo X`nfoo bar`n" "X bar`nfoo X`nfoo X`n"
Case 'amp-remember-empty-pattern' "foo bar`nfoo bar`n" @('/foo/',':1s//X/','/bar/') ':2&' "X bar`nfoo bar`n" "X bar`nX bar`n"
Case 'amp-empty-replacement' "foo bar`nfoo bar`n" @(':1s/foo//','/bar/') ':2&' " bar`nfoo bar`n" " bar`n bar`n"
Case 'amp-captures' "ab`nab`n" @(':1s/\(a\)\(b\)/\2\1/') ':2&' "ba`nab`n" "ba`nba`n"
Case 'amp-replacement-whole-match' "foo`nfoo`n" @(':1s/foo/[&]/') ':2&' "[foo]`nfoo`n" "[foo]`n[foo]`n"
Case 'amp-case-conversion' "foo`nfoo`n" @(':1s/\(foo\)/\U\1/') ':2&' "FOO`nfoo`n" "FOO`nFOO`n"
Case 'amp-alternate-delimiter' "a/b`na/b`n" @(':1s#a/b#X#','/b/') ':2&' "X`na/b`n" "X`nX`n"
Case 'amp-escaped-delimiter' "a/b`na/b`n" @(':1s/a\/b/X/','/b/') ':2&' "X`na/b`n" "X`nX`n"
Case 'amp-magic-retained' "foo`nfoo`n" @(':set magic',':1s/f.o/X/',':set nomagic','/foo/') ':2&' "X`nfoo`n" "X`nX`n"
Case 'amp-unterminated-last-line' "foo`nfoo" @(':1s/foo/X/') ':2&' "X`nfoo" "X`nX"
Case 'amp-multiline-replacement' "foo`nfoo`n" @(':1s/foo/a\%0ab/') ':3&' "a`nb`nfoo`n" "a`nb`na`nb`n"
Case 'amp-uses-mark-address' "foo`nfoo`n" @(':2ka',':1s/foo/X/') ":'a&" "X`nfoo`n" "X`nX`n"
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Case ('amp-'+$enc[0]) "中é😀`r`n中é😀`r`n" @(':1s/中/X/') ':2&' "Xé😀`r`n中é😀`r`n" "Xé😀`r`nXé😀`r`n" $enc[1]
 Case ('tilde-'+$enc[0]) "中é😀`r`n中é😀`r`n" @(':1s/中/X/','/é/') ':2~' "Xé😀`r`n中é😀`r`n" "Xé😀`r`n中X😀`r`n" $enc[1]
}
$long='a'*200
Case 'amp-long-history' ($long+"`n"+$long+"`n") @((':1s/'+$long+'/X/'),'/a/') ':2&' ("X`n"+$long+"`n") "X`nX`n"
Case 'amp-page-boundary' (('a'*8191)+"foo`nfoo`n") @(':1s/foo/X/') ':2&' (('a'*8191)+"X`nfoo`n") (('a'*8191)+"X`nX`n")
Case 'amp-after-global-substitute' "foo`nfoo`n" @(':1g/foo/s/foo/X/','/X/') ':2&' "X`nfoo`n" "X`nX`n"
Case 'amp-after-bare-substitute' "foo bar`nfoo bar`nfoo bar`n" @(':1s/foo/X/','/bar/',':2s','/foo/') ':3&' "X bar`nfoo X`nfoo bar`n" "X bar`nfoo X`nfoo X`n"
foreach($useSearch in $false,$true){
 $name=if($useSearch){'tilde-confirm-skip'}else{'amp-confirm-replace'}
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){continue}
 $v=$null
 try{
  $text=if($useSearch){"foo foo`nbar bar`n"}else{"foo foo`nfoo foo`n"};$before=if($useSearch){"X foo`nbar bar`n"}else{"X foo`nfoo foo`n"};$after=if($useSearch){"X foo`nbar X`n"}else{"X foo`nX X`n"}
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$text,$utf8);$v=Start-WV $path;Ex $v ':1s/foo/X/';if($useSearch){Ex $v '/bar/'}
  Menu $v 119;Chars $v ':';$ed=[WV]::GetDlgItem($v.H,161);[WV]::SetText($ed,$(if($useSearch){':2~gc'}else{':2&gc'}));[void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  foreach($button in @($(if($useSearch){100}else{1}),1)){
   $limit=(Get-Date).AddSeconds(10);do{$dlg=Dialog $v;if($dlg -eq [IntPtr]::Zero){Start-Sleep -Milliseconds 50}}while($dlg -eq [IntPtr]::Zero -and (Get-Date) -lt $limit)
   if($dlg -eq [IntPtr]::Zero){throw 'Missing substitution confirmation'}
   [void][WV]::Send($dlg,0x111,$button,0)
   $limit=(Get-Date).AddSeconds(10);do{$next=Dialog $v;if($next -eq $dlg){Start-Sleep -Milliseconds 50}}while($next -eq $dlg -and (Get-Date) -lt $limit)
   if($next -eq $dlg){throw 'Confirmation did not close'}
  }
  Wait-WVQuiet $v;if((Dialog $v) -ne [IntPtr]::Zero){throw 'Unexpected extra confirmation'};Save-WV $v;Equal-Bytes $path $utf8.GetBytes($after)
  Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes($before);Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes($after)
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-repeat-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Repeat substitution application regressions failed'}
"$($results.Count) repeat-substitution application cases passed"
