param([int]$GiB=17,[switch]$DenseLines,[switch]$KeepFile,[switch]$ReuseInput)
. "$PSScriptRoot\common.ps1"
[WV]::MessageTimeout=120000; [WV]::MessageFlags=0; $env:WINVI_TEST_IDLE_SECONDS='3600'
$path=Join-Path $cases "large-$GiB-GiB.txt"
$watch=[Diagnostics.Stopwatch]::StartNew()
$line=if($DenseLines){"a`n"}else{('a'*1022)+"`r`n"}
$chunk=[Text.Encoding]::ASCII.GetBytes($line*(1048576/$line.Length))
$blocks=[long]$GiB*1024
$offsets=@(0L,4294967296L,17179869184L,([long]$GiB*1073741824+8192-3)) | Where-Object {$_ -lt ([long]$GiB*1073741824+8192)}
if($ReuseInput) {
 $restore=[IO.File]::OpenWrite($path)
 try{$restore.SetLength([long]$GiB*1073741824+8192);foreach($position in $offsets){[void]$restore.Seek($position,[IO.SeekOrigin]::Begin);$restore.WriteByte($chunk[$position%$chunk.Length])}}finally{$restore.Dispose()}
}
$hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
$f=if($ReuseInput){$null}else{[IO.File]::Open($path,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)}
try {
 for($i=0L;$i -lt $blocks;$i++){
  if($f){$f.Write($chunk,0,$chunk.Length)};$base=$i*$chunk.Length
  $special=@($offsets|Where-Object {$_ -ge $base -and $_ -lt $base+$chunk.Length})
  if($special.Count){$expectedChunk=[byte[]]$chunk.Clone();foreach($pos in $special){$expectedChunk[$pos-$base]=88};$hash.AppendData($expectedChunk)}else{$hash.AppendData($chunk)}
 }
 $tail=[Text.Encoding]::ASCII.GetBytes($line*(8192/$line.Length));if($f){$f.Write($tail,0,$tail.Length)};$tail[$tail.Length-3]=88;$hash.AppendData($tail)
}finally{if($f){$f.Dispose()}}
$payload=[Text.Encoding]::ASCII.GetBytes("tail`r`n");$hash.AppendData($payload);$expected=[Convert]::ToHexString($hash.GetHashAndReset());$hash.Dispose()
"Created $((Get-Item $path).Length) bytes in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
$v=$null
try {
 $v=Start-WV $path;Start-Sleep -Seconds 2;Wait-WVQuiet $v 3600;"Loaded in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
 "Toggling hex at $($watch.Elapsed.TotalSeconds)";$d=Dialog $v;if($d -ne [IntPtr]::Zero){$detail=([WV]::Children($d)|ForEach-Object {[WV]::Text($_)}) -join " | ";throw "Unexpected dialog: $([WV]::Text($d)): $detail"};Menu $v 158
 foreach($offset in $offsets){"Editing byte offset $offset";
  Ex $v ':1'
  if($offset -ge 16){Chars $v (([long][math]::Floor($offset/16)).ToString()+'j')}
  $rem=$offset%16;if($rem){Chars $v ($rem.ToString()+'l')}
  Menu $v 118;Chars $v '58';Menu $v 119
 }
 "Toggling hex at $($watch.Elapsed.TotalSeconds)";Menu $v 158
 $insert=Join-Path $cases 'large-insert.txt';[IO.File]::WriteAllBytes($insert,$payload)
 Chars $v 'G$';'Reached final line';Ex $v (':r '+$insert);'Inserted read payload';Chars $v 'u';'Undid insertion';Chars $v 'u';'Redid insertion'
 # Wait for the Save handler itself; a blocked I/O can look idle before Save completes.
 [WV]::MessageTimeout=3600000
 Menu $v 103
 [WV]::MessageTimeout=120000
 Wait-WVQuiet $v 3600
 "Saved in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
 if((Get-Item $path).Length -ne ([long]$GiB*1073741824+8192+$payload.Length)){throw 'Unexpected saved length'}
 $stream=[IO.File]::OpenRead($path);try{$actual=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($stream))}finally{$stream.Dispose()}
 if($actual -ne $expected){throw "Hash mismatch actual=$actual expected=$expected"}
 if((Get-Item $v.Log).Length){throw "Application stderr: $(Get-Content $v.Log -Raw)"}
 $result=[pscustomobject]@{Test='Full open, boundary hex edits, read insertion, undo/redo, save and full SHA256';Bytes=(Get-Item $path).Length;DenseLines=[bool]$DenseLines;Lines=([long]$GiB*1073741824+8192)/$line.Length;Result='PASS';Seconds=[math]::Round($watch.Elapsed.TotalSeconds,2);SHA256=$actual;ExecutableSHA256=(Get-FileHash $exe).Hash}
 $result|ConvertTo-Json|Set-Content (Join-Path $PSScriptRoot "large-$GiB-GiB-results.json");$result|Format-List
}finally{Stop-WV $v;if(!$KeepFile -and (Test-Path $path)){Remove-Item -LiteralPath $path}}
