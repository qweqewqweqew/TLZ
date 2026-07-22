$ErrorActionPreference = 'Stop'

$task = 'CodexSqlSystemFix'
$root = 'D:\QtProj\tongliiz\MzTLZ\downloads'
$payload = Join-Path $root 'sql_system_fix.ps1'
$output = Join-Path $root 'sql_system_fix_output.txt'
$log = Join-Path $root 'sql_system_fix_launcher_log.txt'

Start-Transcript -Path $log -Force
Remove-Item -LiteralPath $output -Force -ErrorAction SilentlyContinue
cmd.exe /c "schtasks /Delete /TN $task /F >nul 2>nul" | Out-Null

$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$payload`""
$trigger = New-ScheduledTaskTrigger -Once -At ([datetime]::Today.AddHours(23).AddMinutes(59))
$principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest

Register-ScheduledTask -TaskName $task -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
Start-ScheduledTask -TaskName $task

for ($i = 0; $i -lt 30; $i++) {
    if (Test-Path $output) {
        Stop-Transcript
        exit 0
    }
    Start-Sleep -Seconds 2
}

Stop-Transcript
exit 1
