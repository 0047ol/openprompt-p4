"""Inventory actual shared-controller notification entry points, without executing them."""
from pathlib import Path
import json
import re

root = Path(__file__).resolve().parents[1]
sources = sorted(path for path in (root/'web').glob('*.js'))
routes = {'message': 'Toast', 'statusToast': 'Toast', 'statusBanner': 'Banner', 'confirmAction': 'Dialog', 'showModal': 'Dialog'}
sites = []
for path in sources:
    for number, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        if line.lstrip().startswith(('//', 'function ', 'async function ')):
            continue
        kinds = [kind for name, kind in routes.items() if re.search(r'\b'+name+r'\(', line)]
        if not kinds:
            continue
        sites.append({'file': path.relative_to(root).as_posix(), 'line': number, 'types': sorted(set(kinds)), 'source': line.strip()})
report = {'source': 'shared web/*.js, excluding generated/demo/native files', 'entryPoints': len(sites), 'sites': sites, 'functionalTestsRun': False}
out = root/'docs/NOTIFICATION-CALLS.json'
out.write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
print(json.dumps({'entryPoints': len(sites), 'file': out.relative_to(root).as_posix()}))
