Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes

$root = [System.Windows.Automation.AutomationElement]::RootElement
$windows = $root.FindAll([System.Windows.Automation.TreeScope]::Children, [System.Windows.Automation.Condition]::TrueCondition)

foreach ($win in $windows) {
    $name = $win.Current.Name
    if ($name -match "Conquer" -or $name -match "Edge" -or $name -match "Drop Unlock") {
        Write-Host "Name: $name | Class: $($win.Current.ClassName) | Rect: $($win.Current.BoundingRectangle)"
    }
}
