[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$wc = New-Object System.Net.WebClient
Write-Host "Downloading VS Build Tools..."
$wc.DownloadFile('https://aka.ms/vs/17/release/vs_BuildTools.exe', 'D:\vs_BuildTools.exe')
Write-Host "Done:" (Get-Item 'D:\vs_BuildTools.exe').Length
