from pathlib import Path
root=Path(__file__).resolve().parents[1]
pm=(root/'pmwin.c').read_text()
loader=(root/'os2host32.c').read_text()
assert 'case WM_SIZE:' in pm and 'WM_SIZE -> guest' in pm
assert 'resource_shadow' in loader
assert 'immutable per-object shadow' in loader
assert '(obj->resource_shadow ? obj->resource_shadow : obj->mapped)' in loader
print('M31D BIO WM_SIZE/resource-shadow regression PASS')
