"""Package P4 firmware and one combined test/handoff document; never flash."""
import hashlib
import json
from pathlib import Path
import shutil
import zipfile
import argparse
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--handoff', '--test-plan', dest='test_plan', default='docs/TEST-PLAN.md', help='Combined findings and test/handoff document')
parser.add_argument('--revision', type=int, default=1, help='Delivery revision for documentation-only updates; keeps the firmware version unchanged')
parser.add_argument('--tested', action='store_true', help='The exact image was installed and tested; see the handoff for outcomes and remaining checks')
args = parser.parse_args()
if args.revision < 1:
    parser.error('--revision must be positive')
build = root / 'firmware/p4/build-idf61'
metadata = json.loads((root / 'releases/latest.json').read_text(encoding='utf8'))
version = metadata['version']
bundle_name = 'teleprompter-p4-' + version + '-review'
suffix = '' if args.revision == 1 else '-r' + str(args.revision)
archive = root / 'releases' / (bundle_name + suffix + '.zip')
archived_release = root / 'archive/releases/p4' / version / archive.name
if archive.exists() or archived_release.exists():
    raise SystemExit(f'Delivery {archive.name} already exists. Increment the firmware version for code changes, or --revision for documentation-only changes.')
workspace = tempfile.TemporaryDirectory(prefix='teleprompter-review-')
target = Path(workspace.name) / bundle_name
target.mkdir(exist_ok=True)
sources = {
    metadata['file']: root / 'releases' / metadata['file'],
    'bootloader.bin': build / 'bootloader/bootloader.bin',
    'partition-table.bin': build / 'partition_table/partition-table.bin',
    'ota_data_initial.bin': build / 'ota_data_initial.bin',
    'TEST-AND-HANDOFF.md': root / args.test_plan,
}
for name, source in sources.items():
    shutil.copy2(source, target / name)
assert hashlib.sha256((target / metadata['file']).read_bytes()).hexdigest() == metadata['sha256']

flash = r'''param(
    [Parameter(Mandatory=$true)][string]$Port,
    [string]$Python='py',
    [int]$Baud=921600
)
$ErrorActionPreference='Stop'
$serial=Get-CimInstance Win32_SerialPort | Where-Object { $_.DeviceID -eq $Port }
if(!$serial -or $serial.PNPDeviceID -notmatch '^USB\\VID_303A&PID_0012&') {
    throw 'Select the P4 ROM USB port. Hold BOOT, tap RST, then release BOOT. Never select the C6 port.'
}
if($Baud -notin @(460800,921600)) { throw 'Use 460800 or 921600 baud.' }
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw | ConvertFrom-Json
foreach($entry in $manifest.images.PSObject.Properties) {
    $actual=(Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $entry.Name) -Algorithm SHA256).Hash.ToLowerInvariant()
    if($actual -ne $entry.Value) { throw ('SHA256 mismatch: '+$entry.Name) }
}
Push-Location $PSScriptRoot
try {
    & $Python -m esptool --chip esp32p4 --port $Port --baud $Baud --before no-reset --after hard-reset write-flash --flash-mode dio --flash-freq 40m --flash-size 16MB 0x2000 bootloader.bin 0x8000 partition-table.bin 0x10000 APP_IMAGE 0xc00000 ota_data_initial.bin
    if($LASTEXITCODE -ne 0) { throw 'Flashing did not complete. Save output; do not erase flash.' }
} finally { Pop-Location }
'''.replace('APP_IMAGE', metadata['file'])
(target / 'flash.ps1').write_text(flash, encoding='utf-8-sig')
images = {name: hashlib.sha256((target / name).read_bytes()).hexdigest()
          for name in sources if name.endswith('.bin')}
(target / 'manifest.json').write_text(json.dumps({**metadata, 'bundleRevision': args.revision,
    'handoffFile': 'TEST-AND-HANDOFF.md',
    'handoffSha256': hashlib.sha256((target / 'TEST-AND-HANDOFF.md').read_bytes()).hexdigest(),
    'status': 'device-tested' if args.tested else 'review-unvalidated', 'images': images}, indent=2)+'\n', encoding='utf8')
(target / 'SHA256SUMS.txt').write_text(''.join(f'{digest}  {name}\n' for name, digest in images.items()), encoding='ascii')
validation = 'Compiled and installed on the P4. Read TEST-AND-HANDOFF.md for the exact completed checks and remaining items.' if args.tested else 'Compiled and packaged; NOT flashed or functionally tested in this delivery.'
(target / 'README.md').write_text(f'''# P4 {version} review build

{validation}
Read TEST-AND-HANDOFF.md: it combines previous test findings, this build's changes,
remaining tests, steps, expected results, reporting, and settings to restore.
Delivery revision: {args.revision}. A new delivery revision alone does not change the firmware.

## Existing controller / SD update

Select `{metadata['file']}` in the controller firmware update page.
Alternatively copy it to `/firmware/{metadata['file']}` on SD and use the device's SD update action.
The other .bin files in this package are USB-only; do not upload them as an OTA application.

## USB on another Windows computer

Requires Python and esptool (build machine uses esptool 5.6.0).
If needed: `py -m pip install esptool==5.6.0`.
Close serial monitors. Enter P4 ROM download mode: hold BOOT, tap and release RST, then release BOOT.
Check the current P4 ROM COM port and run, for example:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM11
```

COM11 is an example, not a fixed device number. Use `-Baud 460800` if 921600 is unreliable.
Use `-Python C:\\path\\to\\python.exe` when using a specific Python environment.
This script writes the existing project's 16 MB P4 layout and resets the OTA boot selector.
It does not erase NVS, the second app slot, or SD. Do not use with another board/layout or flash the C6.
After flashing, save serial output at 115200 baud and run the test plan.

Online publishing is separate; this packaging command does not publish or change latest.json.
App/native resources and the matching APK's validation status are covered in TEST-AND-HANDOFF.md.
APKs are distributed separately from this firmware ZIP.
''', encoding='utf8')
# A dotted version is part of the directory name; append .zip rather than replacing it.
with zipfile.ZipFile(archive, 'x', compression=zipfile.ZIP_DEFLATED) as output:
    for path in sorted(target.iterdir()):
        if path.is_file():
            output.write(path, arcname=target.name + '/' + path.name)
workspace.cleanup()
print(archive)
print(json.dumps(metadata, ensure_ascii=False))
