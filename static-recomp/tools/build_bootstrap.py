"""Incremental native build; private generated C and compiler temporaries stay outside Git."""
import argparse,concurrent.futures,hashlib,os,pathlib,re,subprocess,shlex,sys
p=argparse.ArgumentParser();p.add_argument('--tested-eh',action='store_true',help='explicitly enable the experimentally validated EH runtime');p.add_argument('--h5',type=pathlib.Path,required=True);p.add_argument('--lifted',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];out=a.output.resolve();lift=a.lifted.resolve();h5=a.h5.resolve()
if out==r or r in out.parents or out==lift/'lifted.c' or out==lift/'runtime_config.h':p.error('Build output must stay outside Git and preserve generated inputs')
cache=out.parent/'bootstrap-build';cache.mkdir(parents=True,exist_ok=True);tmp=cache/'tmp';tmp.mkdir(exist_ok=True);env=dict(os.environ,TMPDIR=str(tmp))
code=(lift/'lifted.c').read_text()
# shortcut: the EH dispatcher is not connected; remove this guard after EH execution tests pass.
if 'RECOMP_EH_ENTER();' in code and not a.tested_eh:raise SystemExit('EH landing pads require an EH-capable runtime; bootstrap dispatch is not implemented')
matches=list(re.finditer(r'^void sub_([0-9A-F]{8})\(void\) \{',code,re.M))
if not matches:raise SystemExit('No generated function bodies')
last=code.index('\nvoid h4_',matches[-1].end());groups={}
for i,m in enumerate(matches):
 end=matches[i+1].start() if i+1<len(matches) else last
 groups.setdefault(int(m[1],16)>>14,[]).append(code[m.start():end])
# Declarations may grow without changing the ABI of existing void(void) functions.
(cache/'all_functions.h').write_text('\n'.join(f'void sub_{m[1]}(void);' for m in matches)+'\n')
prefix='#include "recomp_types.h"\n#include "all_functions.h"\n'
sources={f'chunk-{k:04x}':prefix+'\n'.join(v) for k,v in groups.items()};sources['dispatch']=prefix+code[last:]
for name in ['poc_runtime','guest_memory','guest_strings','pe_version','version_selftest','win32_window','ddraw_com','ddraw_selftest','gdi','gdi_selftest','registry','registry_selftest','files','files_selftest','menu_selftest','eh_runtime','bootstrap']:sources[name]=(r/f'static-recomp/native/{name}.c').read_text()
sources['surface_bridge_cpp']=(r/'static-recomp/native/surface_bridge.cpp').read_text()
sources['ddraw_surface_cpp']=(r/'static-recomp/native/ddraw_surface.cpp').read_text()
sources['surface565_cpp']=(r/'src/h4/graphics/surface565.cpp').read_text()
cxx='clang++' if sys.platform=='darwin' else 'g++-14'
flags=['-O1','-g','-fwrapv','-fno-strict-aliasing','-ffp-contract=off','-DRECOMP_GENERATED_CODE','-DRECOMP_TRACE','-DH4_BOOTSTRAP_RUNTIME',f'-DH4_DLMALLOC_SOURCE="{h5}/reference/dlmalloc/malloc.c"','-include',str(lift/'runtime_config.h'),'-I'+str(h5/'android/runtime'),'-I'+str(r/'static-recomp/native'),'-I'+str(cache),'-I'+str(r/'src'),'-pthread']
if a.tested_eh:flags+=['-DH4_TESTED_EH']
flags+=shlex.split(subprocess.check_output(['pkg-config','--cflags','sdl3'],text=True))
sdl_libs=shlex.split(subprocess.check_output(['pkg-config','--static','--libs','sdl3'],text=True))
key=(lift/'runtime_config.h').read_bytes()+(h5/'android/runtime/recomp_types.h').read_bytes()+(r/'static-recomp/native/h4_host.h').read_bytes()+(h5/'reference/dlmalloc/malloc.c').read_bytes()+subprocess.check_output(['clang','--version'])+subprocess.check_output([cxx,'--version'])+repr(flags).encode()
key+=subprocess.check_output(['pkg-config','--modversion','sdl3'])
for header in ['static-recomp/native/surface_bridge.h','static-recomp/native/ddraw_surface.hpp','src/h4/graphics/surface565.hpp','src/h4/graphics/color565.hpp','src/h4/core/geometry.hpp']:
 key+=(r/header).read_bytes()
def compile_one(item):
 name,source=item;digest=hashlib.sha256(key+source.encode()).hexdigest();obj=cache/f'{name}-{digest}.o'
 if obj.exists():return obj,False
 cpp=name.endswith('_cpp');src=cache/f"{name}.{'cpp' if cpp else 'c'}";src.write_text(source);pending=cache/f'{name}.pending.o';subprocess.run([cxx if cpp else 'clang',*flags,*(['-std=c++23'] if cpp else []),'-c',str(src),'-o',str(pending)],check=True,env=env);os.replace(pending,obj);return obj,True
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:results=list(pool.map(compile_one,sources.items()))
linked=cache/'startup-linked';subprocess.run([cxx,*[str(x[0]) for x in results],'-pthread','-lm',*sdl_libs,'-o',str(linked)],check=True,env=env);os.replace(linked,out)
print(f'ARM64 build: {sum(x[1] for x in results)} compiled, {sum(not x[1] for x in results)} cached',flush=True)
print(f'startup executable={out} sha256={hashlib.sha256(out.read_bytes()).hexdigest()}',flush=True)
