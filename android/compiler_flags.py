import json,pathlib
out={}
for p in pathlib.Path('android/app/.cxx').rglob('compile_commands.json'):
    if 'prefab' in str(p): continue
    entries=json.loads(p.read_text())
    selected=[e for e in entries if e['file'].endswith(('src/h4/app/application.cpp','src/h4/graphics/canvas.cpp','src/h4/scene/map_render.cpp'))]
    if selected: out[str(p)]=selected
assert out,'No actual native compiler commands found'
print(json.dumps(out,indent=2))
