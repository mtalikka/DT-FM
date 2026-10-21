param(
    [Parameter(Position = 0)]
    [string] $StockOs
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($StockOs)) {
    throw "Usage: .\scripts\docker-build.ps1 <path-to-stock-Digitakt-OS.syx>"
}

if (-not (Test-Path -LiteralPath $StockOs -PathType Leaf)) {
    throw "Stock OS file not found: $StockOs"
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$stockPath = (Resolve-Path -LiteralPath $StockOs).Path
$imageName = "dt-fm-builder"

docker build --tag $imageName $repoRoot
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

docker run --rm `
    --mount "type=bind,source=$repoRoot,target=/workspace" `
    --mount "type=bind,source=$stockPath,target=/tmp/stock.syx,readonly" `
    $imageName /bin/sh scripts/build.sh /tmp/stock.syx /opt/elekloader
exit $LASTEXITCODE