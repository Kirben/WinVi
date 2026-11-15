. "$PSScriptRoot\common.ps1"
$path=Join-Path $cases 'large-4GiB.txt';$watch=[Diagnostics.Stopwatch]::StartNew()
$line=('a'*1022)+"`r`n";$chunk=[Text.Encoding]::ASCII.GetBytes($line*1024)
$hash=[Security.Cryptography.IncrementalHash]::CreateHash([Security.Cryptography.HashAlgorithmName]::SHA256)
$f=[IO.File]::Open($path,[IO.FileMode]::Create,[IO.FileAccess]::Write,[IO.FileShare]::Read)
try {for($i=0;$i -lt 4096;$i++){$f.Write($chunk,0,$chunk.Length);$hash.AppendData($chunk)};$tail=[Text.Encoding]::ASCII.GetBytes($line*8);$f.Write($tail,0,$tail.Length);$tail[$tail.Length-3]=88;$hash.AppendData($tail)}finally{$f.Dispose()}
$expected=[Convert]::ToHexString($hash.GetHashAndReset());$hash.Dispose();"Created $((Get-Item $path).Length) bytes in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
$v=$null
try {
 $v=Start-WV $path;"Loaded file in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
 Menu $v 119;Chars $v 'G$';Menu $v 118;Chars $v 'X';Menu $v 119
 [void][WV]::PostMessageW($v.H,0x111,[UIntPtr]103,[IntPtr]0);Wait-WVQuiet $v 600
 "Saved file in $([math]::Round($watch.Elapsed.TotalSeconds,1))s"
 $stream=[IO.File]::OpenRead($path);$sha=[Security.Cryptography.SHA256]::Create();try{$actual=[Convert]::ToHexString($sha.ComputeHash($stream))}finally{$stream.Dispose();$sha.Dispose()}
 if($actual -ne $expected){throw "Large file hash mismatch: actual=$actual expected=$expected"}
 $result=[pscustomobject]@{Test='Open, edit final line beyond 4 GiB, save, full SHA-256 verification';Bytes=(Get-Item $path).Length;Result='PASS';Seconds=[math]::Round($watch.Elapsed.TotalSeconds,2);SHA256=$actual}
 $result|ConvertTo-Json|Set-Content (Join-Path $PSScriptRoot 'large-file-4GiB-results.json');$result|Format-List
}finally{Stop-WV $v}

