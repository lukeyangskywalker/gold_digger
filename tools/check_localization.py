"""Check the Simplified Chinese table in platforms/passport/main/ui/lang.c:
unique keys, matching printf tokens, and font coverage of every glyph."""
import json
import re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
ui = root / 'platforms/passport/main/ui'
pairs = [(json.loads(f'"{a}"'), json.loads(f'"{b}"')) for a, b in
         re.findall(r'^\s*\{"((?:\\.|[^"\\])*)",\s*"((?:\\.|[^"\\])*)"\},', (ui / 'lang.c').read_text(encoding='utf-8'), re.M)]
assert pairs, 'no phrases found'
tokens = lambda s: re.findall(r'%(?:%|[-+0-9.]*[hlz]*[a-zA-Z])', s)
seen = set()
for en, zh in pairs:
    assert en not in seen, f'duplicate key: {en}'
    seen.add(en)
    assert tokens(en) == tokens(zh), (en, zh)
font = (ui / 'zh_font.h').read_text(encoding='utf-8')
start = int(re.search(r'\.range_start=(\d+)', font).group(1))
covered = {int(x) + start for x in re.findall(r'\d+', re.search(r'gd_zh_unicode\[\]=\{(.*?)\};', font, re.S).group(1))}
needed = {ord(c) for f in ui.glob('*.c') for c in f.read_text(encoding='utf-8') if ord(c) > 127}
missing = ''.join(chr(c) for c in sorted(needed - covered))
assert not missing, 'missing font glyphs, run tools/pack_zh_font.py: ' + missing
print(f'PASS {len(pairs)} Chinese phrases, printf tokens and {len(needed)} font glyphs')
