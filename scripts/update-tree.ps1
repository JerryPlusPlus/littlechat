$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputPath = Join-Path $projectRoot 'docs/project-tree.txt'
$excluded = @('.git', '.vs', '.idea', '.vscode', 'build', 'out', 'test-temp')
$treeLines = [System.Collections.Generic.List[string]]::new()

function Add-TreeEntries([string]$directory, [string]$prefix) {
    $children = @(Get-ChildItem -LiteralPath $directory -Force |
        Where-Object { $_.Name -notin $excluded } |
        Sort-Object @{ Expression = { -not $_.PSIsContainer } }, Name)
    for ($index = 0; $index -lt $children.Count; $index++) {
        $entry = $children[$index]
        $last = $index -eq $children.Count - 1
        $branch = if ($last) { '\-- ' } else { '|-- ' }
        $suffix = if ($entry.PSIsContainer) { '/' } else { '' }
        $treeLines.Add($prefix + $branch + $entry.Name + $suffix)
        if ($entry.PSIsContainer -and -not ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            $indent = if ($last) { '    ' } else { '|   ' }
            Add-TreeEntries $entry.FullName ($prefix + $indent)
        }
    }
}

if (-not (Test-Path -LiteralPath $outputPath)) {
    New-Item -ItemType File -Path $outputPath -Force | Out-Null
}
$treeLines.Add('Project tree (generated; Git/IDE caches and build output omitted)')
$treeLines.Add('littlechat/')
Add-TreeEntries $projectRoot ''
[IO.File]::WriteAllLines($outputPath, $treeLines, [Text.UTF8Encoding]::new($false))
Write-Host "Updated: $outputPath"
