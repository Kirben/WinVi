. "$PSScriptRoot\common.ps1"
function Ready-FileDialog($v) {
 $limit=(Get-Date).AddSeconds(12);$found=[IntPtr]0
 do {
  foreach($h in [WV]::Windows($v.P.Id)) {
   if([WV]::Class($h) -ne '#32770'){continue}
   if([WV]::Text($h) -eq 'Location is not available') {[void][WV]::Send($h,0x111,1,0);Start-Sleep -Milliseconds 200;continue}
   foreach($ch in [WV]::Children($h)){if([WV]::Class($ch) -eq 'Edit' -and [WV]::GetDlgCtrlID($ch) -eq 1148){$found=$h}}
  }
  Start-Sleep -Milliseconds 150
 }while($found -eq 0 -and (Get-Date) -lt $limit)
 if($found -eq 0){throw 'File dialog not ready'};return $found
}
$path=Join-Path $cases 'dialog-base.txt';$dir=Join-Path $cases 'dialog-long';while($dir.Length -lt 450){$dir=Join-Path $dir ('folder-'+('y'*40))};[void][IO.Directory]::CreateDirectory($dir);$long=Join-Path $dir 'long.txt';[IO.File]::WriteAllText($path,"base`r`n");[IO.File]::WriteAllText($long,"long`r`n")
$v=Start-WV $path
try {
 [void][WV]::PostMessageW($v.H,0x111,[UIntPtr]102,[IntPtr]0);Start-Sleep -Milliseconds 1000;Wait-WVQuiet $v 30;$d=Ready-FileDialog $v
 # Allow shell startup errors to settle before entering a path.
 Start-Sleep -Milliseconds 600;$d=Ready-FileDialog $v
 $ed=[IntPtr]0;foreach($h in [WV]::Children($d)){if([WV]::Class($h) -eq 'Edit' -and [WV]::GetDlgCtrlID($h) -eq 1148){$ed=$h}}
 [WV]::SetText($ed,$long);[void][WV]::PostMessageW([WV]::GetDlgItem($d,1),0xF5,[UIntPtr]0,[IntPtr]0);Start-Sleep -Milliseconds 1000
 $limit=(Get-Date).AddSeconds(8);do{foreach($h in [WV]::Windows($v.P.Id)){if([WV]::Text($h) -eq 'Location is not available'){[void][WV]::Send($h,0x111,1,0)}};Start-Sleep -Milliseconds 200;$remaining=Dialog $v}while($remaining -ne 0 -and (Get-Date) -lt $limit)
 if($remaining -ne 0){throw 'Open dialog remained open'}
 Wait-WVQuiet $v;Menu $v 117;Chars $v 'X';Menu $v 119;Save-WV $v;Equal-Bytes $long ([Text.Encoding]::ASCII.GetBytes("Xlong`r`n"));'PASS native file dialog opens >260-character path'
}finally{Stop-WV $v}
