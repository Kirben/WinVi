. "$PSScriptRoot\common.ps1"
$path=Join-Path $cases 'locked-read.txt';$export=Join-Path $cases 'partial-export.txt'
$bytes=[Text.Encoding]::ASCII.GetBytes(("a"*1022+"`r`n")*4096)
[IO.File]::WriteAllBytes($path,$bytes)
$hash=(Get-FileHash $path).Hash
$locked=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
$v=$null
try {
 $locked.Lock(1048576,4096)
 $v=Start-WV $path
 $d=Dialog $v;if(!$d){throw 'Expected injected read error dialog'}
 [void][WV]::PostMessageW($d,0x10,[UIntPtr]0,[IntPtr]0);Start-Sleep -Milliseconds 300;Wait-WVQuiet $v
 $locked.Unlock(1048576,4096);$locked.Dispose();$locked=$null
 Save-WV $v
 if((Get-FileHash $path).Hash -ne $hash){throw 'Partial read overwrote original'}
 # A hard link must also be protected, even if the target path is different.
 $alias=Join-Path $cases 'locked-alias.txt';if(Test-Path $alias){Remove-Item -LiteralPath $alias}
 New-Item -ItemType HardLink -Path $alias -Target $path | Out-Null
 Ex $v (':w! '+$alias)
 if((Get-FileHash $path).Hash -ne $hash){throw 'Alias bypassed incomplete-read protection'}
 if(Test-Path $export){Remove-Item -LiteralPath $export}
 Ex $v (':w! '+$export)
 if((Get-Item $export).Length -ne 1048576){throw 'Unexpected partial export length'}
 # Reopen successfully, then saving is permitted again.
 Ex $v (':e! '+$path);Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v
 if((Get-Item $path).Length -ne $bytes.Length+1){throw 'Successful reopen remained blocked'}
 $r=[pscustomobject]@{Test='Injected sharing-lock read failure; source/hard-link preservation, partial Save As and successful reopen';Result='PASS'}
 $r|ConvertTo-Json|Set-Content (Join-Path $PSScriptRoot 'read-failure-results.json');$r|Format-List
}finally{if($locked){$locked.Dispose()};Stop-WV $v}
