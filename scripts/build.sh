#!/bin/sh
# Build the S034 DT-FM release without redistributing firmware.
set -eu

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 STOCK_DIGITAKT_OS_1.53_OR_1.54.syx ELEKLOADER_CHECKOUT" >&2
    exit 2
fi

stock=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
loader=$(cd "$2" && pwd)
project=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
python=${PYTHON_BIN:-python3}

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
    1.53) core="$loader/mods/core/out/core-2.1.elemod" ;;
    1.54) core="$loader/mods/core/out/core-2.1-os1.54.elemod" ;;
    *) echo "Expected a Digitakt mk1 OS 1.53 or 1.54 file, not $os." >&2; exit 2 ;;
esac
dtfm="$project/out/dt-fm-1.1.0-os$os.elemod"
health="$project/diagnostics/digihealth/out/digihealth-1.0.1-os$os.elemod"

cd "$loader"
"$python" -m elekloader.sdk.build mods/core --stock "$stock"
"$python" -m elekloader.sdk.build "$project/src" --stock "$stock" --out "$project/out"
(
    cd "$project/diagnostics/digihealth"
    PYTHONPATH="$loader${PYTHONPATH:+:$PYTHONPATH}" "$python" build.py --stock "$stock"
)
# elekloader adds -os<version> only for a port's OS; name 1.53's files alike.
if [ "$os" = 1.53 ]; then
    mv -f "$project/out/dt-fm-1.1.0.elemod" "$dtfm"
    mv -f "$project/diagnostics/digihealth/out/digihealth-1.0.1.elemod" "$health"
fi

"$python" -m elekloader.lint --stock "$stock" "$core" "$health" "$dtfm"
"$python" -m elekloader.patch --stock "$stock" \
    --mod "$core" --mod "$health" --mod "$dtfm" \
    --out "$project/out/Digitakt_OS${os}_DT-FM_S034.syx" --version S034
