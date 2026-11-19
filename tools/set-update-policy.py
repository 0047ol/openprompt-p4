"""Set the force-update flag in a LOCAL manifest. Publish it explicitly afterwards."""
import argparse,json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--manifest',default='releases/latest.json')
parser.add_argument('--force-update',choices=['on','off'],required=True)
args=parser.parse_args()
path=Path(args.manifest)
metadata=json.loads(path.read_text(encoding='utf-8'))
if metadata.get('project')!='teleprompter_p4' or metadata.get('chip')!='esp32p4':
    raise SystemExit('Not a P4 manifest; nothing changed.')
metadata['forceUpdate']=args.force_update=='on'
temporary=path.with_name(path.name+'.new')
temporary.write_text(json.dumps(metadata,indent=2)+'\n',encoding='utf-8')
temporary.replace(path)
print(json.dumps({'version':metadata['version'],'forceUpdate':metadata['forceUpdate'],'published':False}))
