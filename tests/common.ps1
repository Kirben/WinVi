Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Diagnostics; using System.Runtime.InteropServices;
public class WV {
 [StructLayout(LayoutKind.Sequential)] public struct IO {public ulong ReadOps,WriteOps,OtherOps,ReadBytes,WriteBytes,OtherBytes;}
 [DllImport("kernel32.dll")] public static extern bool GetProcessIoCounters(IntPtr h,out IO c);
 [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
 [DllImport("user32.dll")] public static extern bool AreDpiAwarenessContextsEqual(IntPtr a,IntPtr b);
 [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
 public delegate bool CB(IntPtr h,IntPtr l);
 [DllImport("user32.dll")] static extern bool EnumThreadWindows(uint id,CB cb,IntPtr l);
 [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr h,CB cb,IntPtr l);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetClassNameW(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool SetWindowTextW(IntPtr h,string s);
 [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h,int id);
 [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr h,uint m,UIntPtr w,IntPtr l);
 [DllImport("user32.dll",SetLastError=true)] static extern IntPtr SendMessageTimeoutW(IntPtr h,uint m,UIntPtr w,IntPtr l,uint f,uint t,out UIntPtr result);
 [DllImport("user32.dll")] public static extern uint RegisterWindowMessageW([MarshalAs(UnmanagedType.LPWStr)] string s);
 public static void SetText(IntPtr h,string text) {IntPtr p=Marshal.StringToHGlobalUni(text);try{Send(h,0xC,0,p.ToInt64());}finally{Marshal.FreeHGlobal(p);}}
 public static string Class(IntPtr h) {var s=new StringBuilder(256);GetClassNameW(h,s,256);return s.ToString();}
 public static string Text(IntPtr h) {var s=new StringBuilder(2048);GetWindowTextW(h,s,2048);return s.ToString();}
 public static IntPtr[] Windows(int pid) { var a=new List<IntPtr>(); using(var p=Process.GetProcessById(pid)) foreach(ProcessThread t in p.Threads) EnumThreadWindows((uint)t.Id,(h,l)=>{a.Add(h);return true;},IntPtr.Zero);return a.ToArray(); }
 public static IntPtr[] Children(IntPtr root) {var a=new List<IntPtr>();EnumChildWindows(root,(h,l)=>{a.Add(h);return true;},IntPtr.Zero);return a.ToArray();}
 public static ulong Send(IntPtr h,uint m,ulong w,long l) {UIntPtr r;if(SendMessageTimeoutW(h,m,new UIntPtr(w),new IntPtr(l),2,5000,out r)==IntPtr.Zero)throw new Exception("Message timeout "+m);return r.ToUInt64();}
}
"@
$ErrorActionPreference='Stop'
$script:exe=if($env:WINVI_TEST_EXE){$env:WINVI_TEST_EXE}else{Join-Path $PSScriptRoot '..\..\WinVi64.exe'}
$script:cases=Join-Path $PSScriptRoot 'cases'
New-Item -ItemType Directory -Force $cases | Out-Null
function Wait-WVQuiet($v,[int]$seconds=120) {
 $deadline=(Get-Date).AddSeconds($seconds);$quiet=0;$last=$null
 do {Start-Sleep -Milliseconds 200;if($v.P.HasExited){throw "Application exited: $($v.P.ExitCode)"};$io=[WV+IO]::new();[void][WV]::GetProcessIoCounters($v.P.Handle,[ref]$io);$v.P.Refresh();$now="$($io.ReadBytes):$($io.WriteBytes):$($v.P.TotalProcessorTime.Ticks)";if($now -eq $last){$quiet++}else{$quiet=0};$last=$now}while($quiet -lt 3 -and (Get-Date) -lt $deadline)
 if($quiet -lt 3){throw 'Application did not become idle'}
}
function Start-WV($path) {
 $err=Join-Path $cases ('stderr-'+[guid]::NewGuid().ToString()+'.log')
 $p=Start-Process -FilePath $exe -ArgumentList ('"'+$path+'"') -WindowStyle $(if($env:WINVI_TEST_VISIBLE){'Normal'}else{'Hidden'}) -PassThru -RedirectStandardError $err
 $h=[IntPtr]0; $limit=(Get-Date).AddSeconds(15)
 do { Start-Sleep -Milliseconds 100; if($p.HasExited){throw "Exited: $($p.ExitCode)"}; foreach($wh in [WV]::Windows($p.Id)) {if([WV]::Class($wh) -eq "Ramo's WinVi"){$h=$wh}} } while($h -eq 0 -and (Get-Date) -lt $limit)
 if($h -eq 0){$p.Kill();throw 'Missing main window'}
 $v=@{P=$p;H=$h;Log=$err};Wait-WVQuiet $v;return $v
}
function Menu($v,$id) {[void][WV]::Send($v.H,0x111,$id,0)}
function Chars($v,$s) {foreach($c in $s.ToCharArray()){[void][WV]::Send($v.H,0x102,[uint32][char]$c,1)}}
function Ex($v,$s) {Menu $v 119; Chars $v ':'; $ed=[WV]::GetDlgItem($v.H,161); if($ed -eq 0){throw 'Command edit unavailable'}; [WV]::SetText($ed,$s); [void][WV]::Send($v.H,2258,0,0)}
function Save-WV($v) {Menu $v 103; Start-Sleep -Milliseconds 100}
function Stop-WV($v) {if($null -eq $v){return}; if(!$v.P.HasExited){[void][WV]::PostMessageW($v.H,0x10,[UIntPtr]0,[IntPtr]0);if(!$v.P.WaitForExit(1000)){$v.P.Kill();$v.P.WaitForExit()}}}
function Dialog($v) {foreach($wh in [WV]::Windows($v.P.Id)){if([WV]::Class($wh) -eq '#32770'){return $wh}};return [IntPtr]0}
function Equal-Bytes($path,[byte[]]$expected) { $actual=[IO.File]::ReadAllBytes($path); if([Convert]::ToBase64String($actual) -ne [Convert]::ToBase64String($expected)){throw "Bytes differ: actual=$($actual.Length), expected=$($expected.Length)"} }









