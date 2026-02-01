$root = Resolve-Path "."
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error 'git not found in PATH'
    exit 1
}
if (-not (Get-Command clang-format -ErrorAction SilentlyContinue)) {
    Write-Error 'clang-format not found in PATH'
    exit 1
}

$paths = & git -C $root ls-files -z -co --exclude-standard
$files = $paths -split "`0" | Where-Object { $_ -match '\.(c|cc|cpp|h|hpp)$' }

foreach ($f in $files) {
    if ([string]::IsNullOrWhiteSpace($f)) { continue }
    $full = Join-Path $root $f
    Write-Host "Formatting $full"
    & clang-format -i $full
}
