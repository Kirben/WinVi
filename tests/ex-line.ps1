. "$PSScriptRoot/common.ps1"
[WV]::MessageTimeout=30000
$results=[Collections.Generic.List[object]]::new()
$utf8=[Text.UTF8Encoding]::new($false)
function Case($name,$inputText,$commands,$expected,[Text.Encoding]$encoding=$utf8,[bool]$undo=$true,[string]$savePath=''){
 if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}
 $v=$null;$phase='open'
 try{
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllText($path,$inputText,$encoding);$original=[IO.File]::ReadAllBytes($path);$v=Start-WV $path
  foreach($cmd in $commands){$phase=$cmd;if($cmd.StartsWith(':')){Ex $v $cmd}else{Menu $v 119;Chars $v $cmd};Wait-WVQuiet $v}
  if($savePath){$path=$savePath;$original=[byte[]]@()}
  $phase='save';Save-WV $v;$want=$encoding.GetPreamble()+$encoding.GetBytes($expected);Equal-Bytes $path $want
  if($undo){$phase='undo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $original;$phase='redo';Chars $v 'u';Save-WV $v;Equal-Bytes $path $want}
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL $name ($phase): $($_.Exception.Message)"}
 finally{Stop-WV $v}
}
Case 'join-default' "one`ntwo`nthree`n" @(':join') "one two`nthree`n"
Case 'join-range' "one`ntwo`nthree`nfour`n" @(':2,3j') "one`ntwo three`nfour`n"
Case 'join-force' "one `n `ttwo`n" @(':1,2join!') "one  `ttwo`n"
Case 'join-empty-middle' "one`n`ntwo`n" @(':%j') "one two`n"
Case 'join-punctuation' "(`nword`n)`n" @(':%join') "(word)`n"
Case 'join-count' "a`nb`nc`nd`n" @(':join 2') "a b c`nd`n"
Case 'join-range-count' "a`nb`nc`nd`n" @(':1,2join 2') "a`nb c`nd`n"
Case 'join-unterminated' "a`nb" @(':join') 'a b'
Case 'join-one-line-range' "a`nb`n" @(':2,2join') "a`nb`n" $utf8 $false
Case 'join-utf8' "éλ`n中`n" @(':join') "éλ 中`n" ([Text.UTF8Encoding]::new($true))
Case 'join-emoji' "😀`nword`n" @(':join') "😀 word`n" ([Text.UTF8Encoding]::new($true))
Case 'join-utf16le' "é`r`n中`r`n" @(':join') "é 中`r`n" ([Text.UnicodeEncoding]::new($false,$true))
Case 'join-utf16be' "é`r`n中`r`n" @(':join') "é 中`r`n" ([Text.UnicodeEncoding]::new($true,$true))
Case 'yank-put-default' "a`nb`n" @(':yank',':put') "a`na`nb`n"
Case 'yank-range-put-zero' "a`nb`nc`n" @(':2,3yank',':0put') "b`nc`na`nb`nc`n"
Case 'yank-count' "a`nb`nc`n" @(':1yank 2',':$pu') "a`nb`nc`na`nb`n"
Case 'yank-range-count' "a`nb`nc`nd`n" @(':1,2yank 2',':0put') "b`nc`na`nb`nc`nd`n"
Case 'yank-named' "a`nb`n" @(':1ya a',':2yank b',':0put a') "a`na`nb`n"
Case 'yank-append' "a`nb`nc`n" @(':1y a',':3y A',':0put a') "a`nc`na`nb`nc`n"
Case 'yank-last-no-eol' "a`nb" @(':2yank',':$put') "a`nb`nb`n"
Case 'yank-append-no-eol' "a`nb" @(':2yank a',':1yank A',':0put a') "b`na`na`nb"
Case 'yank-preserves-cursor' "a`nb`nc`n" @(':3',':1yank',':put') "a`nb`nc`na`n"
Case 'put-leaves-last-line' "a`nb`nc`nd`n" @(':1,2yank',':4put',':yank x',':0put x') "b`na`nb`nc`nd`na`nb`n" $utf8 $false
Case 'put-character-register' "abc`ndef`n" @('0yl',':0put') "a`nabc`ndef`n"
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Case ('yank-put-'+$enc[0]) "é`r`n中`r`n" @(':1yank',':2put') "é`r`n中`r`né`r`n" $enc[1]
 Case ('yank-vi-put-'+$enc[0]) "é`r`n中`r`n" @(':1yank','p') "é`r`né`r`n中`r`n" $enc[1]
}
Case 'join-page-boundary' (('a'*4095)+"`nlast`n") @(':join') (('a'*4095)+" last`n")
Case 'yank-page-boundary' (('a'*4095)+"`nlast`n") @(':1yank',':2put') (('a'*4095)+"`nlast`n"+('a'*4095)+"`n")
$empty=Join-Path $cases 'empty-put-target.txt';[IO.File]::WriteAllBytes($empty,[byte[]]@())
Case 'put-empty-buffer' "a`n" @(':1yank',(':edit '+$empty),':0put') "a`n" $utf8 $true $empty
Case 'transfer-move-down' "a`nb`nc`nd`n" @(':1,2move 4') "c`nd`na`nb`n"
Case 'transfer-move-up' "a`nb`nc`nd`n" @(':3,4m0') "c`nd`na`nb`n"
Case 'transfer-move-middle' "a`nb`nc`nd`ne`n" @(':2,3move 4') "a`nd`nb`nc`ne`n"
Case 'transfer-copy-down' "a`nb`nc`n" @(':1,2copy $') "a`nb`nc`na`nb`n"
Case 'transfer-copy-up' "a`nb`nc`n" @(':2,3co0') "b`nc`na`nb`nc`n"
Case 'transfer-copy-overlap' "a`nb`nc`n" @(':1,2t1') "a`na`nb`nb`nc`n"
Case 'transfer-copy-self' "a`nb`n" @(':1t1') "a`na`nb`n"
Case 'transfer-default-source' "a`nb`nc`n" @(':2',':t0') "b`na`nb`nc`n"
Case 'transfer-relative-destination' "a`nb`nc`nd`n" @(':2',':1copy +1') "a`nb`nc`na`nd`n"
Case 'transfer-search-destination' "a`nb`nc`n" @(':1copy /b/') "a`nb`na`nc`n"
Case 'transfer-mark-destination' "a`nb`nc`n" @(':2','ma',":3copy 'a") "a`nb`nc`nc`n"
Case 'transfer-percent' "a`nb`n" @(':%t0') "a`nb`na`nb`n"
Case 'transfer-noop' "a`nb`nc`n" @(':2,3m1') "a`nb`nc`n" $utf8 $false
Case 'transfer-copy-unterminated' "a`nb" @(':2t0') "b`na`nb"
Case 'transfer-copy-after-unterminated' "a`nb" @(':1copy $') "a`nb`na`n"
Case 'transfer-move-unterminated-up' "a`nb" @(':2m0') "b`na`n"
Case 'transfer-move-after-unterminated' "a`nb" @(':1m$') "b`na`n"
Case 'transfer-empty-lines' "`n`na`n" @(':1,2m$') "a`n`n`n"
Case 'transfer-seam-delete' "a`rb`n`nc`n" @(':2m$') "a`r`n`nc`nb`n"
Case 'transfer-seam-insert-prefix' "a`rb`n`nc`n" @(':3t1') "a`r`n`nb`n`nc`n"
Case 'transfer-seam-insert-suffix' "a`r`nb`rc`n" @(':2t0') "b`ra`r`nb`rc`n"
Case 'transfer-seam-before-empty-lf' "a`r`nb`rc`n`nd`n" @(':2t3') "a`r`nb`rc`nb`r`n`nd`n"
Case 'transfer-preserve-register' "a`nb`nc`n" @(':2yank',':1move 3',':0put') "b`nb`nc`na`n" $utf8 $false
Case 'transfer-copy-cursor' "a`nb`nc`n" @(':1,2t3',':yank z',':0put z') "b`na`nb`nc`na`nb`n" $utf8 $false
Case 'transfer-move-cursor' "a`nb`nc`nd`n" @(':1,2m4',':yank z',':0put z') "b`nc`nd`na`nb`n" $utf8 $false
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Case ('transfer-copy-'+$enc[0]) "é`r`n中`r`n😀`r`n" @(':2,3t0') "中`r`n😀`r`né`r`n中`r`n😀`r`n" $enc[1]
 Case ('transfer-move-'+$enc[0]) "é`r`n中`r`n😀" @(':3m0') "😀`r`né`r`n中`r`n" $enc[1]
 Case ('transfer-seam-delete-'+$enc[0]) "é`r中`n`n😀`n" @(':2m$') "é`r`n`n😀`n中`n" $enc[1]
}
Case 'transfer-copy-page-boundary' (('a'*8191)+"`nb`nc`n") @(':1,2t$') (('a'*8191)+"`nb`nc`n"+('a'*8191)+"`nb`n")
Case 'transfer-move-page-boundary' (('a'*8191)+"`nb`nc`n") @(':1,2m$') ("c`n"+('a'*8191)+"`nb`n")
Case 'transfer-copy-large-undo' (('a'*131071)+"`nb`nc`n") @(':1,2t$') (('a'*131071)+"`nb`nc`n"+('a'*131071)+"`nb`n")
Case 'transfer-move-large-undo' (('a'*131071)+"`nb`nc`n") @(':1,2m$') ("c`n"+('a'*131071)+"`nb`n")
Case 'indent-right-default' "a`nb`n" @(':set ts=8 sw=4',':>') "    a`nb`n"
Case 'indent-left-default' "      a`nb`n" @(':set ts=8 sw=4',':<') "  a`nb`n"
Case 'indent-right-range' "a`n  b`nc`n" @(':set ts=8 sw=4',':1,2>') "    a`n      b`nc`n"
Case 'indent-left-range' "`ta`n  b`nc`n" @(':set ts=8 sw=4',':1,2<') "    a`nb`nc`n"
Case 'indent-repeated-right' "a`nb`n" @(':set ts=8 sw=4',':1,2>>') "`ta`n`tb`n"
Case 'indent-repeated-left' "`t  a`n`tb`n" @(':set ts=8 sw=4',':1,2<<') "  a`nb`n"
Case 'indent-count' "a`nb`nc`nd`n" @(':set ts=8 sw=4',':2>2') "a`n    b`n    c`nd`n"
Case 'indent-range-count' "a`nb`nc`nd`n" @(':set ts=8 sw=4',':1,2>2') "a`n    b`n    c`nd`n"
Case 'indent-count-clips-eof' "a`nb`n" @(':set ts=8 sw=4',':2>99') "a`n    b`n"
Case 'indent-percent' "a`nb`n" @(':set ts=8 sw=4',':%>') "    a`n    b`n"
Case 'indent-spaces-only' "a`nb`n" @(':set ts=1 sw=4',':1,2>') "    a`n    b`n"
Case 'indent-nondefault-widths' "  a`n`t b`n" @(':set ts=4 sw=3',':1,2>') "`t a`n`t`tb`n"
Case 'indent-blank-lines' "`n `t`na`n" @(':set ts=8 sw=4',':%>') "`n`n    a`n"
Case 'indent-whitespace-eof' "a`n   " @(':set ts=8 sw=4',':2<') "a`n"
Case 'indent-unterminated' "a`nb" @(':set ts=8 sw=4',':2>') "a`n    b"
Case 'indent-mixed-endings' "a`rb`r`nc`n" @(':set ts=8 sw=4',':%>') "    a`r    b`r`n    c`n"
Case 'indent-no-change' "a`nb`n" @(':set ts=8 sw=4',':%<') "a`nb`n" $utf8 $false
Case 'indent-cursor-last-nonblank' "a`n  b`nc`n" @(':set ts=8 sw=4',':1,2>','rX') "    a`n      X`nc`n" $utf8 $false
Case 'indent-preserve-register' "a`nb`n" @(':set ts=8 sw=4',':2yank',':1>',':0put') "b`n    a`nb`n" $utf8 $false
Case 'indent-page-boundary' (('a'*8191)+"`n  b`n") @(':set ts=8 sw=4',':1,2>') ("    "+('a'*8191)+"`n      b`n")
Case 'indent-large-prefix' ((' '*131071)+"a`nb`n") @(':set ts=8 sw=4',':1<') (("`t"*16383)+"   a`nb`n")
foreach($enc in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 Case ('indent-right-'+$enc[0]) "é`r`n  中😀`r`n" @(':set ts=8 sw=4',':%>') "    é`r`n      中😀`r`n" $enc[1]
 Case ('indent-left-'+$enc[0]) "`té`r`n  中😀" @(':set ts=8 sw=4',':%<') "    é`r`n中😀" $enc[1]
 Case ('mark-'+$enc[0]) "é`r`n  中😀`r`n" @(':2mark a','`a','rX') "é`r`n  X😀`r`n" $enc[1]
}
Case 'mark-current-cursor-preserved' "a`nb`nc`n" @(':3',':1mark a','rX') "a`nb`nX`n"
Case 'mark-short-name' "a`n  b`nc`n" @(':2ma a','`a','rX') "a`n  X`nc`n"
Case 'mark-k-spaced' "a`nb`nc`n" @(':2k z',"'z",'rX') "a`nX`nc`n"
Case 'mark-k-compact' "a`nb`nc`n" @(':2',':ka',':1',"'a",'rX') "a`nX`nc`n"
Case 'mark-first-nonblank' "a`n `tb`nc`n" @(':2mark a','`a','rX') "a`n `tX`nc`n"
Case 'mark-view-only' "a`nb`nc`n" @(':set viewonly',':2ka',':set noviewonly',"'a",'rX') "a`nX`nc`n"
Case 'mark-tracks-shift' "a`n  b`n" @(':set ts=8 sw=4',':2ka',':1,2>','`a','rX') "    a`n      X`n" $utf8 $false
Case 'mark-tracks-insert' "a`nb`n" @(':2ka',':1copy0','`a','rX') "a`na`nX`n" $utf8 $false
Case 'mark-tracks-page-split' (('a'*4000)+"`nb`n") @(':set ts=1 sw=128',':2ka',':1>','`a','rX') ((' '*128)+('a'*4000)+"`nX`n") $utf8 $false
Case 'mark-tracks-insert-at-mark' "a`nb`n" @(':set ts=8 sw=4',':1ka',':1>','`a','rX') "    X`nb`n" $utf8 $false
Case 'mark-tracks-delete' "a`nb`nc`nd`n" @(':3ka',':1,2delete','`a','rX') "X`nd`n" $utf8 $false
Case 'mark-inside-deletion' "a`nb`nc`nd`n" @(':2ka',':1,2delete','`a','rX') "X`nd`n" $utf8 $false
Case 'mark-tracks-multipage-delete' (('a'*8191)+"`nb`nc`n") @(':3ka',':1,2delete','`a','rX') "X`n" $utf8 $false
Case 'mark-tracks-undo' "a`nb`n" @(':set ts=8 sw=4',':2ka',':1,2>',':undo','`a','rX') "a`nX`n" $utf8 $false
Case 'mark-tracks-split-at-mark' (('a'*4095)+"`nb`n") @(':set ts=1 sw=128',':1ka',':1>','`a','rX') ((' '*128)+'X'+('a'*4094)+"`nb`n") $utf8 $false
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-line-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Ex line command regressions failed'}
"$($results.Count) line command cases passed"
