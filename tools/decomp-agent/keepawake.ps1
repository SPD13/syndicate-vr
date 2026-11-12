# Keeps the PC from sleeping while the decomp loop (run.py) is running.
Add-Type -Namespace W -Name P -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetThreadExecutionState(uint f);'
$ES_CONTINUOUS = [uint32]2147483648; $ES_SYSTEM_REQUIRED = [uint32]1
[W.P]::SetThreadExecutionState($ES_CONTINUOUS -bor $ES_SYSTEM_REQUIRED) | Out-Null
while ($true) {
    $p = Get-CimInstance Win32_Process -Filter "Name like 'python%'" |
        Where-Object { $_.CommandLine -match 'run\.py' }
    if (-not $p) { break }
    Start-Sleep -Seconds 60
}
[W.P]::SetThreadExecutionState($ES_CONTINUOUS) | Out-Null
"loop ended, keep-awake released"
