Get-Process | Where-Object { $_.MainWindowTitle.Length -gt 0 } | Select-Object Id, ProcessName, MainWindowTitle | Format-Table -AutoSize
