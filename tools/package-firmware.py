"""Package only the OTA app image; never distribute merged flash/bootloader as OTA."""
from pathlib import Path
import hashlib,json,struct,shutil,argparse
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--force-update',action='store_true',help='Require connected web controllers on older versions to install this release')
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
image=root/'firmware/p4/build-idf61/teleprompter_p4.bin'
cache=image.parent/'CMakeCache.txt'
if cache.exists() and 'TP_SD_READONLY_BACKUP:BOOL=ON' in cache.read_text(encoding='utf8'):
    raise SystemExit('Read-only diagnostic firmware is not a distributable release.')
data=image.read_bytes()
assert data[0]==0xe9 and struct.unpack_from('<H',data,12)[0]==0x12
assert struct.unpack_from('<I',data,32)[0]==0xabcd5432
version=data[48:80].split(b'\0')[0].decode('ascii')
out=root/'releases'/f'teleprompter-p4-{version}.bin'
out.parent.mkdir(exist_ok=True)
if out.exists() and out.read_bytes()!=data:
    raise SystemExit(f'Release {version} already exists with different contents; increment PROJECT_VER before packaging.')
if not out.exists():shutil.copy2(image,out)
metadata={'version':version,'project':'teleprompter_p4','chip':'esp32p4','size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'sdPath':'/firmware/'+out.name}
out.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf8')
print(out)
print(json.dumps(metadata))

release={**metadata,'file':out.name,'forceUpdate':args.force_update}
(root/'releases/latest.json').write_text(json.dumps(release,indent=2)+'\n',encoding='utf8')
