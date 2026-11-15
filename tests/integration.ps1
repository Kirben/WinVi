. "$PSScriptRoot\common.ps1"
$script:results=[Collections.Generic.List[object]]::new()
function Check($name,[scriptblock]$test) { if($env:WINVI_TEST_FILTER -and $name -notmatch $env:WINVI_TEST_FILTER){return}; $script:v=$null; $watch=[Diagnostics.Stopwatch]::StartNew();try { & $test; if($script:v -and $script:v.P.HasExited){throw 'Application exited unexpectedly'}; if($script:v -and (Get-Item $script:v.Log).Length){throw ([IO.File]::ReadAllText($script:v.Log))}; $results.Add([pscustomobject]@{Test=$name;Result='PASS';Detail='';Seconds=[math]::Round($watch.Elapsed.TotalSeconds,2)}); "PASS $name" }catch { $results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message;Seconds=[math]::Round($watch.Elapsed.TotalSeconds,2)}); "FAIL ${name}: $($_.Exception.Message)" }finally {Stop-WV $script:v} }
function Prefix-Case($name,[byte[]]$bytes,[byte[]]$prefix,[int]$bom=0) {
 Check $name {
  $path=Join-Path $cases ($name+'.txt');[IO.File]::WriteAllBytes($path,$bytes)
  $script:v=Start-WV $path; Menu $v 117; Chars $v 'X';Menu $v 119;Save-WV $v
  $expected=[Collections.Generic.List[byte]]::new();if($bom){$expected.AddRange([byte[]]$bytes[0..($bom-1)])};$expected.AddRange($prefix);if($bytes.Length -gt $bom){$expected.AddRange([byte[]]$bytes[$bom..($bytes.Length-1)])}; Equal-Bytes $path $expected.ToArray()
 }
}
$ascii=[Text.Encoding]::ASCII;$utf8=[Text.UTF8Encoding]::new($true);$le=[Text.UnicodeEncoding]::new($false,$true);$be=[Text.UnicodeEncoding]::new($true,$true)
Prefix-Case 'ascii-crlf' $ascii.GetBytes("alpha`r`nbeta`r`n") ([byte[]](88))
Prefix-Case 'ascii-lf' $ascii.GetBytes("alpha`nbeta`n") ([byte[]](88))
Prefix-Case 'no-final-newline' $ascii.GetBytes('alpha') ([byte[]](88))
Prefix-Case 'empty-file' ([byte[]]@()) ([byte[]](88))
Prefix-Case 'utf8-bom' ([byte[]]($utf8.GetPreamble()+$utf8.GetBytes("café 中`r`n"))) ([byte[]](88)) 3
Prefix-Case 'utf8-no-bom' ([Text.UTF8Encoding]::new($false).GetBytes("café 中`n")) ([byte[]](88))
Prefix-Case 'utf16-le' ([byte[]]($le.GetPreamble()+$le.GetBytes("café 中`r`n"))) ([byte[]](88,0)) 2
Prefix-Case 'utf16-be' ([byte[]]($be.GetPreamble()+$be.GetBytes("café 中`r`n"))) ([byte[]](0,88)) 2
Prefix-Case 'unicode-中-é-filename' $ascii.GetBytes("alpha`r`n") ([byte[]](88))
foreach($size in 4095,4096,4097,8192) {Prefix-Case "page-boundary-$size" ($ascii.GetBytes(('a'*($size-2))+"`r`n")) ([byte[]](88))}
Prefix-Case 'paging-2MiB' ($ascii.GetBytes(("0123456789abcdef0123456789abcdef`r`n"*62000))) ([byte[]](88))
Check 'undo-redo' {
 $path=Join-Path $cases 'undo.txt';$orig=$ascii.GetBytes("alpha`r`n");[IO.File]::WriteAllBytes($path,$orig);$script:v=Start-WV $path
 Menu $v 117;Chars $v 'X';Menu $v 119;Chars $v 'u';Save-WV $v;Equal-Bytes $path $orig
 Chars $v 'u';Save-WV $v;Equal-Bytes $path ([byte[]](@(88)+$orig))
}
Check 'search-replace-ex' {
 $path=Join-Path $cases 'replace.txt';[IO.File]::WriteAllText($path,"alpha alpha`r`nalpha`r`n",$ascii);$script:v=Start-WV $path
 Ex $v ':%s/alpha/BETA/g';Save-WV $v;Equal-Bytes $path $ascii.GetBytes("BETA BETA`r`nBETA`r`n")
}
Check 'delete-truncates-file' {
 $path=Join-Path $cases 'truncate.txt';[IO.File]::WriteAllText($path,"alpha`r`nbeta`r`ngamma`r`n",$ascii);$script:v=Start-WV $path
 Ex $v ':2,3d';Save-WV $v;Equal-Bytes $path $ascii.GetBytes("alpha`r`n")
}
Check 'read-insert-file' {
 $path=Join-Path $cases 'read.txt';$insert=Join-Path $cases 'insert.txt';[IO.File]::WriteAllText($path,"alpha`r`n",$ascii);[IO.File]::WriteAllText($insert,"beta`r`n",$ascii);$script:v=Start-WV $path
 Ex $v (':r '+$insert);Save-WV $v;Equal-Bytes $path $ascii.GetBytes("alpha`r`nbeta`r`n")
}
Check 'binary-hex-roundtrip' {
 $path=Join-Path $cases 'binary.bin';$bytes=[byte[]](0..255)*64;[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
 Menu $v 158;Menu $v 158;Save-WV $v;Equal-Bytes $path $bytes
}
Check 'about-native64' {
 $path=Join-Path $cases 'about.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path
 [void][WV]::PostMessageW($v.H,0x111,[UIntPtr]127,[IntPtr]0);Start-Sleep -Milliseconds 200;$d=Dialog $v
 $deadline=(Get-Date).AddSeconds(10);do{$bits=[WV]::Text([WV]::GetDlgItem($d,201));if($bits -eq '(64 bit)'){break};Start-Sleep -Milliseconds 100}while((Get-Date) -lt $deadline);[void][WV]::Send($d,0x111,1,0);if($bits -ne '(64 bit)'){throw "About reports $bits"}
}
Check 'multiple-window-intercom' {
 $path=Join-Path $cases 'multi.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path;$v2=Start-WV $path
 try {$msg=[WV]::RegisterWindowMessageW("WinVi - Message");$a=[WV]::Send($v.H,$msg,[uint64]$v.H.ToInt64(),0);$b=[WV]::Send($v2.H,$msg,[uint64]$v2.H.ToInt64(),0);if($a -ge 20 -or $b -ge 20 -or $a -eq $b){throw "Bad indices $a,$b"}}finally{Stop-WV $v2}
}
Check 'settings-all-tabs-cancel' {
 $path=Join-Path $cases 'settings.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path
 Menu $v 133;Start-Sleep -Milliseconds 150;$d=Dialog $v;if($d -eq 0){throw 'No settings dialog'};$tab=[WV]::GetDlgItem($d,1108);$count=[WV]::Send($tab,0x1304,0,0)
 if($count -lt 8){throw "Missing tabs: $count"}
 for($i=0;$i -lt $count;$i++){[void][WV]::Send($tab,0x1330,$i,0);Start-Sleep -Milliseconds 40;$cur=[WV]::Send($tab,0x130B,0,0);if($cur -ne $i){throw "Tab $i did not select: $cur"};if([WV]::Children($d).Count -lt 10){throw 'Missing settings controls'}}
 [void][WV]::Send($d,0x111,2,0);Start-Sleep -Milliseconds 200;if((Dialog $v) -ne 0){throw 'Settings did not close'}
}
Check 'search-dialog-open-cancel' {
 $path=Join-Path $cases 'search-dialog.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path
 Menu $v 171;Start-Sleep -Milliseconds 100;$d=Dialog $v;if($d -eq 0){throw 'No search dialog'};if([WV]::GetDlgItem($d,1013) -eq 0){throw 'Missing search input'};[void][WV]::Send($d,0x111,2,0)
}
Check 'save-as-ex-new-path' {
 $path=Join-Path $cases 'save-source.txt';$dest=Join-Path $cases 'save-dest.txt';if(Test-Path $dest){Remove-Item $dest};$bytes=$ascii.GetBytes("alpha`r`n");[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
 Ex $v (':w! '+$dest);Equal-Bytes $dest $bytes;Equal-Bytes $path $bytes
}
Check 'new-missing-file' {
 $path=Join-Path $cases 'new-missing.txt';if(Test-Path $path){Remove-Item $path};$script:v=Start-WV $path;Menu $v 117;Chars $v 'new';Menu $v 119;Save-WV $v;Equal-Bytes $path $ascii.GetBytes("new`r`n")
}
Check 'unicode-input-bmp-and-emoji' {
 $path=Join-Path $cases 'unicode-input.txt';$bytes=[byte[]]($utf8.GetPreamble()+$utf8.GetBytes("alpha`r`n"));[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
 Menu $v 117;foreach($cp in 0xe9,0x4e2d,0x1f600,90){[void][WV]::Send($v.H,0x109,$cp,1)};Menu $v 119;Save-WV $v
 $text=([string][char]0xe9)+([string][char]0x4e2d)+[char]::ConvertFromUtf32(0x1f600)+"Zalpha`r`n"
 Equal-Bytes $path ([byte[]]($utf8.GetPreamble()+$utf8.GetBytes($text)))
}
Check 'font-picker-hook-open-cancel' {
 $path=Join-Path $cases 'font.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path
 Menu $v 133;Start-Sleep -Milliseconds 100;$d=Dialog $v;$tab=[WV]::GetDlgItem($d,1108);[void][WV]::Send($tab,0x1330,8,0)
 $page=[IntPtr]0;foreach($ch in [WV]::Children($d)){if([WV]::Class($ch) -eq '#32770' -and [WV]::GetDlgItem($ch,1144) -ne 0){$page=$ch}}
 if($page -eq 0){throw 'Font page missing'}
 [void][WV]::PostMessageW($page,0x111,[UIntPtr]1144,[IntPtr]0);Start-Sleep -Milliseconds 800
 $font=[IntPtr]0;foreach($wh in [WV]::Windows($v.P.Id)){if([WV]::Class($wh) -eq '#32770' -and $wh -ne $d){$font=$wh}}
 $deadline=(Get-Date).AddSeconds(5);while($font -eq 0 -and (Get-Date) -lt $deadline){Start-Sleep -Milliseconds 100;foreach($wh in [WV]::Windows($v.P.Id)){if([WV]::Class($wh) -eq '#32770' -and $wh -ne $d){$font=$wh}}};if($font -eq 0){throw 'Font picker missing'};$deadline=(Get-Date).AddSeconds(5);while([WV]::GetDlgItem($font,0x470) -eq 0 -and (Get-Date) -lt $deadline){Start-Sleep -Milliseconds 100};if([WV]::GetDlgItem($font,0x470) -eq 0){throw 'Font family control missing'}
 [void][WV]::Send($font,0x111,2,0);[void][WV]::Send($d,0x111,2,0)
}
foreach($length in 450,1400) {
 Check "long-path-$length-open-edit-save-read-cd" {
  $dir=Join-Path $cases 'long-path-root';while($dir.Length -lt $length){$dir=Join-Path $dir ('nested-'+('x'*43))};[void][IO.Directory]::CreateDirectory($dir)
  $path=Join-Path $dir 'document-中.txt';$bytes=$ascii.GetBytes("alpha`r`n");[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
  Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v;Equal-Bytes $path ([byte[]](@(88)+$bytes))
  $dest=Join-Path $dir 'saved-copy.txt';Ex $v (':w! '+$dest);Equal-Bytes $dest ([byte[]](@(88)+$bytes))
  Ex $v (':cd '+$dir);Ex $v ':e saved-copy.txt';Menu $v 117;Chars $v 'Y';Menu $v 119;Save-WV $v;Equal-Bytes $dest ([byte[]](@(89,88)+$bytes))
 }
}
Check 'enhanced-gdi-dpi-context' {
 $path=Join-Path $cases 'dpi.txt';[IO.File]::WriteAllText($path,"test`r`n");$script:v=Start-WV $path
 $ctx=[WV]::GetWindowDpiAwarenessContext($v.H);if(![WV]::AreDpiAwarenessContextsEqual($ctx,[IntPtr](-5))){throw "Unexpected DPI context: $ctx"}
}
foreach($spec in @(@('utf8',$utf8),@('utf16le',$le),@('utf16be',$be))) {
 Check "supplementary-edit-delete-replace-$($spec[0])" {
  $encoding=$spec[1];$path=Join-Path $cases ('supplementary-'+$spec[0]+'.txt');$base=[byte[]]($encoding.GetPreamble()+$encoding.GetBytes("alpha`r`n"));[IO.File]::WriteAllBytes($path,$base);$script:v=Start-WV $path
  Menu $v 117;[void][WV]::Send($v.H,0x109,0x1f600,1);Chars $v 'Z';Chars $v ([string][char]8);Chars $v ([string][char]8);Menu $v 119;Save-WV $v;Equal-Bytes $path $base
  Ex $v ':e!';Menu $v 117;[void][WV]::Send($v.H,0x102,0xd83d,1);[void][WV]::Send($v.H,0x102,0xde00,1);Menu $v 119;Save-WV $v
  $withEmoji=[byte[]]($encoding.GetPreamble()+$encoding.GetBytes([char]::ConvertFromUtf32(0x1f600)+"alpha`r`n"));Equal-Bytes $path $withEmoji
  Ex $v ':e!';Chars $v 'x';Save-WV $v;Equal-Bytes $path $base
  Ex $v ':e!';Menu $v 117;[void][WV]::Send($v.H,0x109,0x1f600,1);Menu $v 119;Save-WV $v;Ex $v ':e!';Menu $v 118;Chars $v 'X';Menu $v 119;Save-WV $v
  Equal-Bytes $path ([byte[]]($encoding.GetPreamble()+$encoding.GetBytes("Xalpha`r`n")))
 }
}
Check 'hex-edit-nibbles' {
 $path=Join-Path $cases 'hex-edit.bin';$bytes=[byte[]](0..255);[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
 Menu $v 158;Ex $v ':1';Menu $v 118;Chars $v '41';Menu $v 119;Save-WV $v;$bytes[0]=65;Equal-Bytes $path $bytes
}
Check 'address-overflow-rejected' {
 $path=Join-Path $cases 'address-overflow.txt';$bytes=$ascii.GetBytes("alpha`r`n");[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path
 foreach($address in ':9223372036854775808',':18446744073709551616',':999999999999999999999999999999'){Ex $v $address}
 Chars $v '4294967295d2l'
 Save-WV $v;Equal-Bytes $path $bytes
}
Check 'append-sparse-1TiB-and-truncate' {
 $path=Join-Path $cases 'append-source.txt';$big=Join-Path $cases 'append-1TiB.bin';$bytes=$ascii.GetBytes("alpha`r`n");[IO.File]::WriteAllBytes($path,$bytes)
 [IO.File]::WriteAllBytes($big,[byte[]](65));& fsutil sparse setflag $big | Out-Null;if($LASTEXITCODE){throw 'Sparse setup failed'}
 $f=[IO.File]::OpenWrite($big);try{$f.SetLength(1099511627776L);[void]$f.Seek(-1,[IO.SeekOrigin]::End);$f.WriteByte(81)}finally{$f.Dispose()}
 try {
  $script:v=Start-WV $path;Ex $v (':w >> '+$big)
  if((Get-Item $big).Length -ne 1099511627776L+$bytes.Length){throw 'Append size mismatch'}
  $f=[IO.File]::OpenRead($big);try{if($f.ReadByte() -ne 65){throw 'Append changed first byte'};[void]$f.Seek(1099511627775L,[IO.SeekOrigin]::Begin);$tail=[byte[]]::new(8);if($f.Read($tail,0,8) -ne 8 -or [Convert]::ToBase64String($tail) -ne [Convert]::ToBase64String([byte[]](@(81)+$bytes))){throw 'Append data or original endpoint mismatch'}}finally{$f.Dispose()}
  Ex $v (':w! '+$big);Equal-Bytes $big $bytes
 }finally{Remove-Item -LiteralPath $big}
}
$results | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $PSScriptRoot 'integration-results.json')
$failed=@($results|Where-Object Result -eq 'FAIL');"$($results.Count-$failed.Count)/$($results.Count) passed";if($failed.Count){exit 1}















