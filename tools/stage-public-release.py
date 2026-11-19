"""Stage a published release for Pages, verifying the manifest before deployment."""
import argparse
import hashlib
import json
import urllib.parse
import urllib.request
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--repo', required=True)
parser.add_argument('--tag', default='')
parser.add_argument('--output', default='_site/firmware')
args = parser.parse_args()
route = 'tags/' + urllib.parse.quote(args.tag, safe='') if args.tag else 'latest'
request = urllib.request.Request('https://api.github.com/repos/' + args.repo + '/releases/' + route, headers={'Accept': 'application/vnd.github+json', 'User-Agent': 'OpenPrompt-Pages'})
with urllib.request.urlopen(request, timeout=90) as response:
    release = json.load(response)
if release['draft']:
    raise SystemExit('Cannot deploy an unpublished firmware release.')
assets = {asset['name']: asset for asset in release['assets']}

def download(name):
    asset = assets[name]
    # Download public assets without forwarding any authorization to redirects.
    with urllib.request.urlopen(asset['browser_download_url'], timeout=120) as response:
        data = response.read()
    if len(data) != asset['size']:
        raise SystemExit('Incomplete published asset: ' + name)
    if asset.get('digest') and asset['digest'] != 'sha256:' + hashlib.sha256(data).hexdigest():
        raise SystemExit('Published asset checksum mismatch: ' + name)
    return data

manifest = download('latest.json')
metadata = json.loads(manifest)
name = metadata['file']
if Path(name).name != name or '/' in name or '\\' in name or not name.endswith('.bin'):
    raise SystemExit('Invalid application image name.')
data = download(name)
if len(data) != metadata['size'] or hashlib.sha256(data).hexdigest() != metadata['sha256']:
    raise SystemExit('Firmware does not match its manifest.')
if data[48:80].split(bytes([0]))[0].decode() != metadata['version']:
    raise SystemExit('Embedded firmware version differs from manifest.')
if release['tag_name'] != 'v' + metadata['version']:
    raise SystemExit('Release tag differs from firmware version.')
output = Path(args.output)
output.mkdir(parents=True, exist_ok=True)
(output / name).write_bytes(data)
(output / 'latest.json').write_bytes(manifest)
print(json.dumps({'stagedRelease': release['tag_name'], 'sha256': metadata['sha256']}))
