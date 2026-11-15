. "$PSScriptRoot\common.ps1"
$original=[Text.Encoding]::ASCII.GetBytes(('a'*34)+"c`r`n")
$path=Join-Path $cases 'cancel-search.txt'
[IO.File]::WriteAllBytes($path,$original)
$v=$null
try {
 $v=Start-WV $path
 Menu $v 119;Chars $v ':'
 $edit=[WV]::GetDlgItem($v.H,161)
 if($edit -eq [IntPtr]::Zero){throw 'Missing command edit'}
 [WV]::SetText($edit,(':/'+('a*'*24)+'[b]/'))
 [void][WV]::PostMessageW($v.H,2258,[UIntPtr]::Zero,[IntPtr]::Zero)
 Start-Sleep -Milliseconds 500
 $watch=[Diagnostics.Stopwatch]::StartNew()
 [void][WV]::PostMessageW($v.H,0x102,[UIntPtr]3,[IntPtr]1)
 Wait-WVQuiet $v 10
 Ex $v ':1'
 Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v
 Equal-Bytes $path ([byte[]](@(88)+$original))
 if((Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))}
 [pscustomobject]@{Test='Cancel pathological regex via queued Ctrl+C, then edit/save';Result='PASS';RecoveryMilliseconds=$watch.ElapsedMilliseconds;ExecutableSHA256=(Get-FileHash $exe).Hash}|ConvertTo-Json|Tee-Object (Join-Path $PSScriptRoot 'search-cancel-results.json')
}finally{Stop-WV $v}
