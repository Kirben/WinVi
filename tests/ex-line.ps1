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
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-line-results.json"
if(@($results|Where-Object Result -eq 'FAIL').Count){throw 'Ex line command regressions failed'}
"$($results.Count) line command cases passed"
