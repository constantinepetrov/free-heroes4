"""Check H5 EH landing-pad generation using only our own PE32 fixture."""
import hashlib,json,pathlib,struct,subprocess,sys
fixture,h5,out=map(pathlib.Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
raw=bytearray((fixture/'synthetic.pe').read_bytes())
# A conventional MSVC6 registration, an independent catch and a cold continuation.
owner=bytes.fromhex('558bec6aff680012400064a1000000005064892500000000ff150c204000c9c3');raw[0x200:0x200+len(owner)]=owner
raw[0x400:0x40a]=bytes.fromhex('b830204000e906000000')
raw[0x410]=0xc3
raw[0x500:0x506]=bytes.fromhex('b800144000c3')
continuation=bytes.fromhex('8b4df464890d00000000b82a0000005dc3');raw[0x600:0x600+len(continuation)]=continuation
struct.pack_into('<5I',raw,0x830,0x19930520,1,0x402050,1,0x402058)
struct.pack_into('<2I',raw,0x850,0xffffffff,0)
struct.pack_into('<5I',raw,0x858,0,0,1,1,0x402070)
struct.pack_into('<4I',raw,0x870,0,0,0,0x401300)
exe=out/'fixture.pe';exe.write_bytes(raw)
m=json.loads((fixture/'manifest.json').read_text());m['sha256']=hashlib.sha256(raw).hexdigest();m['functions']={'startup':'0x401000'}
manifest=out/'manifest.json';manifest.write_text(json.dumps(m))
repo=pathlib.Path(__file__).resolve().parents[2];lifted=out/'lifted'
subprocess.run([sys.executable,str(repo/'static-recomp/tools/lift_selected.py'),'--h5',str(h5),'--exe',str(exe),'--manifest',str(manifest),'--output',str(lifted),'--synthetic','--eh'],check=True)
code=(lifted/'lifted.c').read_text();start=code.index('void sub_00401000(void) {');end=code.index('void sub_',start+5);owner=code[start:end]
assert 'RECOMP_EH_ENTER()' in owner,owner
assert 'L_00401400:' in owner,owner
assert 'case 0x401300u:' in code,code
# Build the generated bodies in separate units as the incremental builder does.
import re
matches=list(re.finditer(r'^void sub_([0-9A-F]{8})\(void\) \{',code,re.M))
last=code.index('\nvoid h4_',matches[-1].end())
header='\n'.join('void sub_'+m[1]+'(void);' for m in matches)
for i,m in enumerate(matches):
 end=matches[i+1].start() if i+1<len(matches) else last
 unit=out/f'unit-{i}.c';unit.write_text('#include "recomp_types.h"\n'+header+'\n'+code[m.start():end])
 subprocess.run(['clang','-O1','-DRECOMP_GENERATED_CODE','-DRECOMP_TRACE','-DH4_BOOTSTRAP_RUNTIME','-include',str(lifted/'runtime_config.h'),'-I'+str(h5/'android/runtime'),'-c',str(unit),'-o',str(out/f'unit-{i}.o')],check=True)
dispatch=out/'dispatch.c';dispatch.write_text('#include "recomp_types.h"\n'+header+'\n'+code[last:])
flags=['-O1','-DRECOMP_GENERATED_CODE','-DRECOMP_TRACE','-DH4_BOOTSTRAP_RUNTIME','-include',str(lifted/'runtime_config.h'),'-I'+str(h5/'android/runtime')]
subprocess.run(['clang',*flags,str(repo/'static-recomp/native/poc_runtime.c'),str(repo/'static-recomp/tests/eh_landing_host.c'),str(dispatch),*[str(out/f'unit-{i}.o') for i in range(len(matches))],'-lm','-o',str(out/'landing-test')],check=True)
subprocess.run([str(out/'landing-test')],check=True)
# The real bootstrap must fail closed until its EH dispatcher is implemented.
r=subprocess.run([sys.executable,str(repo/'static-recomp/tools/build_bootstrap.py'),'--h5',str(h5),'--lifted',str(lifted),'--output',str(out/'blocked-startup')],text=True,capture_output=True)
assert r.returncode and 'EH-capable runtime' in r.stderr,(r.returncode,r.stderr)
assert not (out/'blocked-startup').exists()
print('PASS synthetic MSVC6 EH owner, catch dispatch, cold landing pad and split-compatible C compilation')
