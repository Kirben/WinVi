. "$PSScriptRoot/common.ps1"
$results=[Collections.Generic.List[object]]::new();$utf8=[Text.UTF8Encoding]::new($false)
function Check($name,[scriptblock]$action,[byte[]]$initial=$null){
 if($env:WINVI_TEST_FILTER -and $name-notmatch$env:WINVI_TEST_FILTER){return}
 $script:v=$null
 try{
  $script:folder=Join-Path $cases ($name+'-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory -Force $folder|Out-Null
  $script:path=Join-Path $folder 'original.txt'
  if($null-eq$initial){[IO.File]::WriteAllText($path,"a`nb`nc`n",$utf8)}else{[IO.File]::WriteAllBytes($path,$initial)}
  $script:v=Start-WV $path;Ex $v (':set tmpdirectory="'+$folder+'"')
  & $action
  if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
  $results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"
 }catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL ${name}: $($_.Exception.Message)"}
 finally{if($v-and!$v.P.HasExited){$v.P.Kill();$v.P.WaitForExit()}}
}
Check 'preserve-original-and-undo' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
 $captures=@(Get-ChildItem (Join-Path $folder 'WinVi-recovery') -Filter '*.wvr')
 if($captures.Count-ne1){throw 'Missing preserved snapshot'}
 Ex $v ':undo';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
Check 'recover-force' {
 Ex $v ':1s/a/A/';Ex $v ':pre';Ex $v ':1s/A/Z/';Ex $v ':rec!'
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n");Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'recover-reject-unsaved' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';Ex $v ':1s/A/Z/';Ex $v ':recover';Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("Z`nb`nc`n")
}
Check 'recover-after-restart' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';$v.P.Kill();$v.P.WaitForExit()
 $script:v=Start-WV $path;Ex $v (':set tmpdirectory="'+$folder+'"');Ex $v ':recover';Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'recover-missing-leaves-undo' {
 Ex $v ':1s/a/A/';Ex $v ':recover!';Ex $v ':undo';Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("a`nb`nc`n")
}
foreach($variant in @(@('utf8',[Text.UTF8Encoding]::new($true)),@('utf16le',[Text.UnicodeEncoding]::new($false,$true)),@('utf16be',[Text.UnicodeEncoding]::new($true,$true)))){
 $encoding=$variant[1]
 Check ('recover-'+$variant[0]) {
  Ex $v ':1s/a/A/';Ex $v ':preserve';Ex $v ':1s/A/Z/';Ex $v ':recover!';Save-WV $v
  Equal-Bytes $path ($encoding.GetPreamble()+$encoding.GetBytes("A`nb`nc`n"))
 } ($encoding.GetPreamble()+$encoding.GetBytes("a`nb`nc`n"))
}
Check 'recover-empty' {
 Ex $v ':preserve';Chars $v ('iX'+[char]27);Ex $v ':recover!';Save-WV $v
 Equal-Bytes $path ([byte[]]::new(0))
} ([byte[]]::new(0))
$binary=[byte[]]::new(12317);for($i=0;$i-lt$binary.Length;$i++){$binary[$i]=[byte]($i%256)}
Check 'recover-binary' {
 Ex $v ':preserve';Ex $v ':%delete';Ex $v ':recover!';Save-WV $v;Equal-Bytes $path $binary
} $binary
Check 'recover-explicit-original' {
 Ex $v ':1s/a/A/';Ex $v ':preserve'
 $other=Join-Path $folder 'other.txt';[IO.File]::WriteAllText($other,"other`n",$utf8)
 Ex $v (':edit! '+$other)
 if([WV]::Text($v.H)-notmatch'other'){throw 'Other buffer was not opened'}
 Ex $v (':recover "'+$path+'"');Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n");Equal-Bytes $other $utf8.GetBytes("other`n")
}
Check 'recover-corrupt-fallback' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';Start-Sleep -Milliseconds 30;Ex $v ':1s/A/Z/';Ex $v ':preserve'
 $latest=Get-ChildItem (Join-Path $folder 'WinVi-recovery') -Filter '*.wvr'|Sort-Object LastWriteTimeUtc -Descending|Select-Object -First 1
 $bytes=[IO.File]::ReadAllBytes($latest.FullName);$bytes[$bytes.Length-1]=$bytes[$bytes.Length-1]-bxor1;[IO.File]::WriteAllBytes($latest.FullName,$bytes)
 Ex $v ':recover!';Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'recover-unnamed' {
 Menu $v 100;Ex $v (':set tmpdirectory="'+$folder+'"');Ex $v ':%delete';Chars $v ('iX'+[char]27)
 Ex $v ':preserve';Ex $v ':1s/X/Z/';Ex $v ':recover!'
 $output=Join-Path $folder 'unnamed-result.txt';Ex $v (':write! '+$output);Equal-Bytes $output $utf8.GetBytes('X')
}
Check 'recover-new-name-overwrite-warning' {
 $target=Join-Path $folder 'new-target.txt';$stamp=[IO.File]::GetLastWriteTimeUtc($path)
 Ex $v (':file '+$target);Ex $v ':preserve'
 [IO.File]::WriteAllText($target,"external`n",$utf8);[IO.File]::SetLastWriteTimeUtc($target,$stamp)
 Ex $v ':recover!'
 if([WV]::Text($v.H)-notmatch'new-target'){throw 'Recovered original name was not restored'}
 [void][WV]::PostMessageW($v.H,0x111,[UIntPtr]::new(103),[IntPtr]::Zero)
 $until=(Get-Date).AddSeconds(10);$dialog=[IntPtr]::Zero
 do{$dialog=Dialog $v;if($dialog-eq[IntPtr]::Zero){Start-Sleep -Milliseconds 50}}while($dialog-eq[IntPtr]::Zero-and(Get-Date)-lt$until)
 if($dialog-eq[IntPtr]::Zero){throw 'Recovery lost the new-name overwrite warning'}
 [void][WV]::Send($dialog,0x111,2,0);Wait-WVQuiet $v
 Equal-Bytes $target $utf8.GetBytes("external`n")
}
Check 'recover-cursor' {
 Ex $v ':2';Ex $v ':preserve';Ex $v ':1';Ex $v ':recover';Chars $v 'rZ';Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("a`nZ`nc`n")
}
Check 'recover-clears-old-undo' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';Ex $v ':1s/A/Z/';Ex $v ':recover!';Ex $v ':undo';Save-WV $v
 Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'recover-repeated-handles' {
 Ex $v ':1s/a/A/';Ex $v ':preserve'
 for($i=0;$i-lt5;$i++){Ex $v ':recover!'};Wait-WVQuiet $v;$v.P.Refresh();$before=$v.P.HandleCount
 for($i=0;$i-lt50;$i++){Ex $v ':recover!'};Wait-WVQuiet $v;$v.P.Refresh()
 if($v.P.HandleCount-gt$before+4){throw 'Repeated recovery leaked handles'}
 Save-WV $v;Equal-Bytes $path $utf8.GetBytes("A`nb`nc`n")
}
Check 'preserve-new-after-partial-load' {
 $partial=Join-Path $folder 'partial.txt';$data=$utf8.GetBytes(('a'*1022+"`r`n")*4096)
 [IO.File]::WriteAllBytes($partial,$data);$locked=[IO.File]::Open($partial,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
 try{
  $locked.Lock(1048576,4096)
  Menu $v 119;Chars $v ':';[WV]::SetText([WV]::GetDlgItem($v.H,161),(':edit! '+$partial))
  [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
  $until=(Get-Date).AddSeconds(15);$dialog=[IntPtr]::Zero
  do{$dialog=Dialog $v;if($dialog-eq[IntPtr]::Zero){Start-Sleep -Milliseconds 50}}while($dialog-eq[IntPtr]::Zero-and(Get-Date)-lt$until)
  if($dialog-eq[IntPtr]::Zero){throw 'Missing injected partial-read dialog'}
  [void][WV]::PostMessageW($dialog,0x10,[UIntPtr]::Zero,[IntPtr]::Zero);Wait-WVQuiet $v
  $locked.Unlock(1048576,4096)
 }finally{$locked.Dispose()}
 Ex $v ':preserve'
 if(Test-Path (Join-Path $folder 'WinVi-recovery')){throw 'Partial load was preserved as complete'}
 Menu $v 100;Ex $v (':set tmpdirectory="'+$folder+'"');Ex $v ':%delete';Chars $v ('iX'+[char]27);Ex $v ':preserve'
 $captures=@(Get-ChildItem (Join-Path $folder 'WinVi-recovery') -Filter '*.wvr')
 if($captures.Count-ne1){throw 'New document retained the partial-read state'}
 Equal-Bytes $partial $data
}
$large=$utf8.GetBytes(('abcdef0123456789'*4096+"`n")*32)
Check 'recover-paged-large' {
 Ex $v ':1s/a/A/';Ex $v ':preserve';Ex $v ':1s/A/Z/';Ex $v ':recover!';Save-WV $v
 $expected=$large.Clone();$expected[0]=65;Equal-Bytes $path $expected
} $large
$results|ConvertTo-Json|Set-Content "$PSScriptRoot/ex-recovery-results.json"
if(@($results|Where-Object {$_.Result-eq'FAIL'}).Count){throw 'Ex recovery application regressions failed'}
"$($results.Count) recovery application cases passed"
