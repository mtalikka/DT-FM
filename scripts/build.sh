#!/bin/sh
# Build the S034 DT-FM release without redistributing firmware. The elekloader
# checkout must contain the current Digitakt core and machine-pages mods.
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 STOCK_DIGITAKT_OS_1.53_OR_1.54.syx ELEKLOADER_CHECKOUT" >&2
    exit 2
fi

stock=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
loader=$(cd "$2" && pwd)
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python=${PYTHON_BIN:-python3}
dtfm_version="1.2.1"

if [ ! -f "$stock" ] || [ ! -f "$loader/elekloader/sdk/build.py" ]; then
    echo "Expected a stock OS file and an elekloader source checkout." >&2
    exit 2
fi

# The OS the stock file is for.
os=$(PYTHONPATH="$loader${PYTHONPATH:+:$PYTHONPATH}" "$python" -c '
import sys
from elekloader import devices, syx
dev, rel = devices.identify(syx.Syx.load(sys.argv[1]).sha256)
print(rel.version if dev.key == "digitakt-mk1" else dev.key)' "$stock")
case "$os" in
    1.53) suffix="" ;;
    1.54) suffix="-os1.54" ;;
    *) echo "Expected a Digitakt mk1 OS 1.53 or 1.54 file, not $os." >&2; exit 2 ;;
esac
dtfm="$project/out/dt-fm-$dtfm_version-os$os.elemod"
health="$project/diagnostics/digihealth/out/digihealth-1.0.1-os$os.elemod"

cd "$loader"
"$python" -m elekloader.sdk.build mods/core --stock "$stock"
"$python" -m elekloader.sdk.build mods/machine-pages --stock "$stock"
"$python" -m elekloader.sdk.build "$project/src" --stock "$stock" --out "$project/out"
(
    cd "$project/diagnostics/digihealth"
    PYTHONPATH="$loader${PYTHONPATH:+:$PYTHONPATH}" "$python" build.py --stock "$stock"
)

# Select the newest artifact for this OS. The core and machine-pages versions
# advance independently, and both remain compatible with DT-FM's core-3.0 API.
newest() {
    files=$(find "$1" -maxdepth 1 -type f -name '*.elemod' | sort -V)
    if [ -n "$suffix" ]; then
        printf '%s\n' "$files" | grep -- "$suffix\\.elemod\$" | tail -1
    else
        printf '%s\n' "$files" | grep -v -- '-os' | tail -1
    fi
}

core=$(newest "$loader/mods/core/out")
pages=$(newest "$loader/mods/machine-pages/out")
if [ -z "$core" ] || [ -z "$pages" ]; then
    echo "Could not find OS-matching core or machine-pages artifacts." >&2
    exit 2
fi

# elekloader adds -os<version> only for a port's OS; name 1.53's files alike.
if [ -z "$suffix" ]; then
    mv -f "$project/out/dt-fm-$dtfm_version.elemod" "$dtfm"
    mv -f "$project/diagnostics/digihealth/out/digihealth-1.0.1.elemod" "$health"
fi

"$python" -m elekloader.lint --stock "$stock" "$core" "$pages" "$health" "$dtfm"
"$python" -m elekloader.patch --stock "$stock" \
    --mod "$core" --mod "$pages" --mod "$health" --mod "$dtfm" \
    --out "$project/out/Digitakt_OS${os}_DT-FM_S034.syx" --version S034
