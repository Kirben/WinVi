. "$PSScriptRoot/common.ps1"
Add-Type @'
using System;using System.Runtime.InteropServices;
public class SplitUi {
 [DllImport("user32.dll")]public static extern IntPtr GetParent(IntPtr h);
 [DllImport("user32.dll")]public static extern bool IsWindowEnabled(IntPtr h);
 [StructLayout(LayoutKind.Sequential)]public struct RECT {public int Left,Top,Right,Bottom;}
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")]public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint flags);
}
'@
$results=[Collections.Generic.List[object]]::new()
function Check($name,[scriptblock]$test){
 $script:v=$null
 try{& $test;if($v -and !$v.P.HasExited -and (Get-Item $v.Log).Length){throw ([IO.File]::ReadAllText($v.Log))};$results.Add([pscustomobject]@{Test=$name;Result='PASS'});"PASS $name"}
 catch{$results.Add([pscustomobject]@{Test=$name;Result='FAIL';Detail=$_.Exception.Message});"FAIL $name : $($_.Exception.Message)"}
 finally{Stop-WV $script:v}
}
function Open-Case($name,[byte[]]$bytes){$script:path=Join-Path $cases "$name.txt";[IO.File]::WriteAllBytes($path,$bytes);$script:v=Start-WV $path;Ex $v ':set nosplitlines splitwidth=10 noautoindent';Menu $v 117}
function Finish($expected,[Text.Encoding]$encoding){Menu $v 119;Save-WV $v;Equal-Bytes $path $encoding.GetBytes($expected)}
$ascii=[Text.Encoding]::ASCII
Check 'disabled-by-default' {Open-Case 'disabled' @();Chars $v 'alpha betaX';Finish 'alpha betaX' $ascii}
Check 'menu-toggle-wrap-and-stop' {Open-Case 'toggle' @();Menu $v 316;Chars $v 'alpha betaX';Menu $v 316;Chars $v ' gamma delta';Finish "alpha`r`nbetaX gamma delta" $ascii}
Check 'exact-width-no-wrap' {Open-Case 'exact' @();Ex $v ':set splitlines';Menu $v 117;Chars $v 'alpha beta';Finish 'alpha beta' $ascii}
Check 'whole-word-preserved' {Open-Case 'word' @();Ex $v ':set splitlines';Menu $v 117;Chars $v 'abcdefghijklmnop';Finish 'abcdefghijklmnop' $ascii}
Check 'long-word-followed-by-word' {Open-Case 'longword-next' @();Menu $v 316;Chars $v 'abcdefghijklmnop next';Finish "abcdefghijklmnop`r`nnext" $ascii}
Check 'repeated-wraps' {Open-Case 'repeat' @();Menu $v 316;Chars $v 'alpha beta gamma delta';Finish "alpha beta`r`ngamma`r`ndelta" $ascii}
Check 'whitespace-run' {Open-Case 'spaces' @();Menu $v 316;Chars $v 'alpha   beta';Finish "alpha`r`nbeta" $ascii}
Check 'tab-columns' {Open-Case 'tabs' @();Ex $v ':set tabstop=8 splitlines';Menu $v 117;Chars $v "alpha`tbeta";Finish "alpha`r`nbeta" $ascii}
Check 'autoindent' {Open-Case 'indent' @();Ex $v ':set autoindent splitlines';Menu $v 117;Chars $v '  alpha betaX';Finish "  alpha`r`n  betaX" $ascii}
Check 'indent-only-not-split' {Open-Case 'indent-only' @();Menu $v 316;Chars $v '            X';Finish '            X' $ascii}
Check 'excess-indent-not-expanded' {Open-Case 'over-indent' @();Ex $v ':set splitlines autoindent';Menu $v 117;Chars $v '            one two';Finish '            one two' $ascii}
Check 'undo-toggle-redo' {Open-Case 'undo' @();Menu $v 316;Chars $v 'alpha betaX';Menu $v 119;Chars $v 'u';Save-WV $v;Equal-Bytes $path @();Chars $v 'u';Save-WV $v;Equal-Bytes $path $ascii.GetBytes("alpha`r`nbetaX")}
Check 'LF-preserved' {Open-Case 'lf' $ascii.GetBytes("`n");Menu $v 316;Chars $v 'alpha betaX';Finish "alpha`nbetaX`n" $ascii}
Check 'CR-preserved' {Open-Case 'cr' $ascii.GetBytes("`r");Menu $v 316;Chars $v 'alpha betaX';Finish "alpha`rbetaX`r" $ascii}
foreach($kind in 'utf8','utf16le','utf16be'){
 Check "$kind-supplementary" {
  $encoding=switch($kind){utf8{[Text.UTF8Encoding]::new($true)}utf16le{[Text.UnicodeEncoding]::new($false,$true)}utf16be{[Text.UnicodeEncoding]::new($true,$true)}}
  Open-Case $kind ([byte[]]($encoding.GetPreamble()+$encoding.GetBytes("`r`n")))
  Menu $v 316;Chars $v 'abc😀 defghi';Menu $v 119;Save-WV $v
  Equal-Bytes $path ([byte[]]($encoding.GetPreamble()+$encoding.GetBytes("abc😀`r`ndefghi`r`n")))
 }
}
Check 'replace-mode-excluded' {Open-Case 'replace' $ascii.GetBytes('000000000000000');Menu $v 316;Menu $v 118;Chars $v 'alpha betaX';Finish '0alpha betaX000' $ascii}
Check 'paste-does-not-reflow' {Open-Case 'paste' $ascii.GetBytes('alpha beta gamma delta');Menu $v 316;Menu $v 120;Menu $v 111;Menu $v 113;Finish 'alpha beta gamma delta' $ascii}
Check 'existing-huge-line-bounded' {Open-Case 'huge' $ascii.GetBytes('x'*200000);Ex $v ':$';Menu $v 119;Chars $v '$';Menu $v 117;Menu $v 316;$timer=[Diagnostics.Stopwatch]::StartNew();Chars $v 'X';if($timer.Elapsed.TotalSeconds-gt5){throw 'Typing in a huge line stalled'};Menu $v 119;Save-WV $v;if((Get-Item $path).Length-ne200001){throw 'Unexpected reflow of existing long line'}}
Check 'width-one' {Open-Case 'one' @();Ex $v ':set splitwidth=1 splitlines';Menu $v 117;Chars $v 'a b c';Finish "a`r`nb`r`nc" $ascii}
Check 'width-thousand' {Open-Case 'thousand' @();Ex $v ':set splitwidth=1000 splitlines';Menu $v 117;Chars $v (('a'*500)+' '+('b'*500));Finish (('a'*500)+"`r`n"+('b'*500)) $ascii}
Check 'line-two-cursor-and-word' {Open-Case 'two' $ascii.GetBytes("first`r`nalpha betX tail`r`n");Ex $v ':2';Menu $v 119;Chars $v '$';Chars $v '0';Menu $v 117;Menu $v 316;Chars $v 'one two three ';Finish "first`r`none two`r`nthree alpha betX tail`r`n" $ascii}
Check 'closing-parenthesis-wrap' {Open-Case 'paren' @();Menu $v 316;Chars $v 'alpha beta)';Finish "alpha`r`nbeta)" $ascii}
Check 'settings-width-apply-and-menu-sync' {
 Open-Case 'settings' @();Menu $v 133;Start-Sleep -Milliseconds 200;$settings=Dialog $v;if(!$settings){throw 'Settings dialog missing'}
 $tab=[WV]::GetDlgItem($settings,1108);[void][WV]::Send($tab,0x1330,3,0);Start-Sleep -Milliseconds 150
 $check=@([WV]::Children($settings)|Where-Object {[WV]::GetDlgCtrlID($_)-eq1173})[0]
 $width=@([WV]::Children($settings)|Where-Object {[WV]::GetDlgCtrlID($_)-eq1174})[0]
 if(!$check -or !$width){throw 'Edit controls missing'}
 if([SplitUi]::IsWindowEnabled($width)){throw 'Width enabled while splitting is off'}
 [void][WV]::Send($check,0xF1,1,0);$page=[SplitUi]::GetParent($check);[void][WV]::Send($page,0x111,1173,$check.ToInt64());[WV]::SetText($width,'6')
 Add-Type -AssemblyName System.Drawing;$rect=[SplitUi+RECT]::new();[void][SplitUi]::GetWindowRect($settings,[ref]$rect)
 $image=[Drawing.Bitmap]::new(($rect.Right-$rect.Left),($rect.Bottom-$rect.Top));$graphics=[Drawing.Graphics]::FromImage($image);$dc=$graphics.GetHdc()
 try{[void][SplitUi]::PrintWindow($settings,$dc,0)}finally{$graphics.ReleaseHdc($dc);$graphics.Dispose()}
 $image.Save((Join-Path $cases 'settings.png'));$image.Dispose()
 [void][WV]::Send([WV]::GetDlgItem($settings,1112),0xF1,0,0);[void][WV]::Send([WV]::GetDlgItem($settings,1113),0xF1,1,0)
 [void][WV]::Send($settings,0x111,1,0);Start-Sleep -Milliseconds 150
 $editMenu=[WV]::GetSubMenu([WV]::GetMenu($v.H),1);[void][WV]::Send($v.H,0x117,[uint64]$editMenu.ToInt64(),1)
 if(!([WV]::GetMenuState($editMenu,316,0)-band8)){throw 'Menu not checked after settings apply'}
 Menu $v 117;Chars $v 'one two';Finish "one`r`ntwo" $ascii
}
$results|ConvertTo-Json -Depth 4|Set-Content "$PSScriptRoot/auto-split-results.json"
if(@($results|Where-Object Result -ne PASS).Count){exit 1}
