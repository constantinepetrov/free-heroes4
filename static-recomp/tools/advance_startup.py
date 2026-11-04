"""Private, bounded on-demand lifting of unresolved main-image functions. Never executes PE."""
import argparse,json,pathlib,re,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('--h5',type=pathlib.Path,required=True);p.add_argument('--exe',type=pathlib.Path,required=True);p.add_argument('--manifest',type=pathlib.Path,required=True);p.add_argument('--image',type=pathlib.Path,required=True);p.add_argument('--work',type=pathlib.Path,required=True);p.add_argument('--steps',type=int,default=40);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];work=a.work.resolve()
if work==r or r in work.parents:p.error('Private output must be outside Git')
work.mkdir(parents=True,exist_ok=True);mp=work/'manifest.json'
if not mp.exists():mp.write_bytes(a.manifest.read_bytes())
import pefile
pe=pefile.PE(str(a.exe));base=pe.OPTIONAL_HEADER.ImageBase
if a.image.read_bytes()!=pe.get_memory_mapped_image():p.error('Mapped image differs from reviewed PE')
ranges=[(base+s.VirtualAddress,base+s.VirtualAddress+s.Misc_VirtualSize) for s in pe.sections if s.Characteristics&0x20000000]
for step in range(a.steps):
 subprocess.run([sys.executable,str(r/'static-recomp/tools/lift_selected.py'),'--h5',str(a.h5),'--exe',str(a.exe),'--manifest',str(mp),'--output',str(work/'lifted')],check=True)
 subprocess.run([sys.executable,str(r/'static-recomp/tools/build_bootstrap.py'),'--h5',str(a.h5),'--lifted',str(work/'lifted'),'--output',str(work/'startup')],check=True)
 try:run=subprocess.run([str(work/'startup'),str(a.image)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=20)
 except subprocess.TimeoutExpired as e:
  print('Startup timeout; preserve diagnostics, do not assume progress');(work/'startup-timeout.txt').write_bytes(e.stdout or b'');break
 (work/f'step-{step:03}.txt').write_text(run.stdout);(work/'last-startup.txt').write_text(run.stdout);print(run.stdout,flush=True)
 m=re.search(r'unsupported dispatch ([0-9a-f]{8})',run.stdout)
 if run.returncode!=77 or not m:break
 va=int(m[1],16)
 if not any(lo<=va<hi for lo,hi in ranges):raise SystemExit('Dispatch outside reviewed main PE text')
 manifest=json.loads(mp.read_text())
 if va in [int(x,0) for x in manifest['functions'].values()]:raise SystemExit('Already lifted target failed dispatch')
 manifest['functions'][f'helper_{va:08x}']=hex(va);mp.write_text(json.dumps(manifest,indent=2)+'\n')
