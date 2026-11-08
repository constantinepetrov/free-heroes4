"""Private, bounded on-demand lifting of unresolved main-image functions. Never executes PE."""
import argparse,json,pathlib,re,subprocess,sys,os
p=argparse.ArgumentParser();p.add_argument('--tested-eh',action='store_true',help='explicitly enable EH after passing synthetic tests');p.add_argument('--eh',action='store_true',help='generate experimental EH landing pads; bootstrap is not yet EH-capable');p.add_argument('--catalog',type=pathlib.Path);p.add_argument('--h5',type=pathlib.Path,required=True);p.add_argument('--exe',type=pathlib.Path,required=True);p.add_argument('--manifest',type=pathlib.Path,required=True);p.add_argument('--image',type=pathlib.Path,required=True);p.add_argument('--work',type=pathlib.Path,required=True);p.add_argument('--steps',type=int,default=40);a=p.parse_args()
if a.tested_eh and not a.eh:p.error('--tested-eh requires --eh')
r=pathlib.Path(__file__).resolve().parents[2];work=a.work.resolve()
if work==r or r in work.parents:p.error('Private output must be outside Git')
work.mkdir(parents=True,exist_ok=True);mp=work/'manifest.json'
if not mp.exists():mp.write_bytes(a.manifest.read_bytes())
import pefile
pe=pefile.PE(str(a.exe));base=pe.OPTIONAL_HEADER.ImageBase
if a.image.read_bytes()!=pe.get_memory_mapped_image():p.error('Mapped image differs from reviewed PE')
ranges=[(base+s.VirtualAddress,base+s.VirtualAddress+s.Misc_VirtualSize) for s in pe.sections if s.Characteristics&0x20000000]
state=work/"userdata";state.mkdir(exist_ok=True);run_env=dict(os.environ,H4_STATE_DIR=str(state))
catalog={}
if a.catalog:
 data=json.loads(a.catalog.read_text())
 if data['image_base']!=base:raise SystemExit('Catalog image base differs from PE')
 catalog={f['address']:f.get('calls_to',[]) for f in data['functions']}
for step in range(a.steps):
 subprocess.run([sys.executable,str(r/'static-recomp/tools/lift_selected.py'),'--h5',str(a.h5),'--exe',str(a.exe),'--manifest',str(mp),'--output',str(work/'lifted'),*(['--synthetic'] if json.loads(mp.read_text()).get('synthetic') else []),*(['--eh'] if a.eh else [])],check=True)
 subprocess.run([sys.executable,str(r/'static-recomp/tools/build_bootstrap.py'),'--h5',str(a.h5),'--lifted',str(work/'lifted'),'--output',str(work/'startup'),*(['--tested-eh'] if a.tested_eh else [])],check=True)
 try:run=subprocess.run([str(work/'startup'),str(a.image)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20,env=run_env)
 except subprocess.TimeoutExpired as e:
  print('Startup timeout; preserve diagnostics, do not assume progress');(work/'startup-timeout.txt').write_bytes(e.stdout or b'');break
 (work/f'step-{step:03}.txt').write_text(run.stdout);(work/'last-startup.txt').write_text(run.stdout);important=[line for line in run.stdout.splitlines() if any(word in line for word in ["unsupported dispatch","unimplemented ","guest fault","lift trap","execution summary:","original MessageBoxA","Captured ","DirectDraw present","RaiseException"])]
 print(" | ".join(important) or f"native exit {run.returncode}; full log: {work/'last-startup.txt'}",flush=True)
 m=re.search(r'unsupported dispatch ([0-9a-f]{8})',run.stdout)
 if run.returncode!=77 or not m:break
 va=int(m[1],16)
 if not any(lo<=va<hi for lo,hi in ranges):raise SystemExit('Dispatch outside reviewed main PE text')
 manifest=json.loads(mp.read_text())
 if va in [int(x,0) for x in manifest['functions'].values()]:raise SystemExit('Already lifted target failed dispatch')
 selected={int(x,0) for x in manifest['functions'].values()};pending={va};additions=set()
 for depth in range(3):
  batch=[v for v in sorted(pending-selected-additions) if any(lo<=v<hi for lo,hi in ranges)][:64-len(additions)]
  additions.update(batch);pending={v for target in batch for v in catalog.get(target,[])}
 for target in sorted(additions):manifest['functions'][f'helper_{target:08x}']=hex(target)
 mp.write_text(json.dumps(manifest,indent=2)+'\n')
