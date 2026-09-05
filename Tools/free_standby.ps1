# Frees the Windows standby list: the cached copies of files that have already been read or
# written. Nothing is lost -- every page in that list is a copy of data that is on disk anyway --
# but it turns tens of gigabytes of "used" memory back into genuinely free memory, so Windows
# stops trimming the working sets of interactive applications to make room.
#
# Needed after a batch that moves a lot of file data (the texture library is 6.6 GB, and a
# measurement pass reads about ten gigabytes on top of that). Requires elevation.
#
#   powershell -File Tools\free_standby.ps1
#
# Reports the free memory before and after.
$ErrorActionPreference = "Stop"
# The elevated instance runs in its own window, so its result goes to a file the caller can read.
$log = Join-Path $env:TEMP "ambient_free_standby.txt"
function Say([string]$t) { Write-Output $t; Add-Content -Path $log -Value $t }

$id = [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Remove-Item $log -ErrorAction SilentlyContinue
    Write-Output "needs administrator rights; restarting elevated (a UAC prompt will appear)..."
    $p = Start-Process powershell -Verb RunAs -PassThru -Wait -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`""
    if (Test-Path $log) { Get-Content $log } else { Write-Output "the elevated run produced no output (cancelled?)" }
    return
}

Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Mm {
  [DllImport("ntdll.dll")] public static extern uint NtSetSystemInformation(int cls, IntPtr info, int len);
  [DllImport("advapi32.dll", SetLastError=true)] public static extern bool OpenProcessToken(IntPtr h, uint access, out IntPtr tok);
  [DllImport("advapi32.dll", SetLastError=true)] public static extern bool LookupPrivilegeValue(string sys, string name, out long luid);
  [DllImport("advapi32.dll", SetLastError=true)] public static extern bool AdjustTokenPrivileges(IntPtr tok, bool disableAll, ref TP newState, int len, IntPtr prev, IntPtr retLen);
  [DllImport("kernel32.dll")] public static extern IntPtr GetCurrentProcess();
  // TOKEN_PRIVILEGES is packed to 4: DWORD count, then LUID (8) and DWORD attributes with no
  // padding. With natural alignment the long lands four bytes too far and the call comes back
  // 0xC0000061, "privilege not held", even though the privilege is right there in the token.
  [StructLayout(LayoutKind.Sequential, Pack=4)] public struct TP { public int Count; public long Luid; public int Attributes; }
  [DllImport("kernel32.dll")] public static extern int GetLastError();
}
"@

function Get-FreeGB { [math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory / 1MB, 1) }
function Get-StandbyGB {
    $m = Get-CimInstance Win32_PerfRawData_PerfOS_Memory
    [math]::Round(($m.StandbyCacheNormalPriorityBytes + $m.StandbyCacheReserveBytes + $m.StandbyCacheCoreBytes) / 1GB, 1)
}

$beforeFree = Get-FreeGB
$beforeStandby = Get-StandbyGB

# SeProfileSingleProcessPrivilege is what NtSetSystemInformation wants for the memory list calls.
$tok = [IntPtr]::Zero
[void][Mm]::OpenProcessToken([Mm]::GetCurrentProcess(), 0x20 -bor 0x8, [ref]$tok)
$luid = 0L
[void][Mm]::LookupPrivilegeValue($null, "SeProfileSingleProcessPrivilege", [ref]$luid)
$tp = New-Object Mm+TP
$tp.Count = 1; $tp.Luid = $luid; $tp.Attributes = 2   # SE_PRIVILEGE_ENABLED
$adjusted = [Mm]::AdjustTokenPrivileges($tok, $false, [ref]$tp, [Runtime.InteropServices.Marshal]::SizeOf($tp), [IntPtr]::Zero, [IntPtr]::Zero)
$err = [Mm]::GetLastError()
if (-not $adjusted -or $err -ne 0) { Say ("AdjustTokenPrivileges: ok={0} lastError={1} (1300 = the privilege is not in the token)" -f $adjusted, $err) }

# SystemMemoryListInformation = 80; command 4 = PurgeStandbyList.
$buf = [Runtime.InteropServices.Marshal]::AllocHGlobal(4)
[Runtime.InteropServices.Marshal]::WriteInt32($buf, 4)
$rc = [Mm]::NtSetSystemInformation(80, $buf, 4)
[Runtime.InteropServices.Marshal]::FreeHGlobal($buf)

Start-Sleep -Milliseconds 700
if ($rc -ne 0) {
    Say ("NtSetSystemInformation returned 0x{0:X} -- the standby list was not purged." -f $rc)
} else {
    Say ("standby {0} -> {1} GB, free {2} -> {3} GB" -f $beforeStandby, (Get-StandbyGB), $beforeFree, (Get-FreeGB))
}
