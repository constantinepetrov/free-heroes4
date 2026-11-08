"""Check H5 EH landing-pad generation using only our own PE32 fixture."""
import hashlib,json,os,pathlib,struct,subprocess,sys
import pefile
fixture,h5,out=map(pathlib.Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
raw=bytearray((fixture/'synthetic.pe').read_bytes())
# A conventional MSVC6 registration, an independent catch and a cold continuation.
prefix=bytes.fromhex('558bec6aff680012400064a100000000506489250000000083ec048965f0c745fc00000000')
owner=prefix+b'\xe8'+struct.pack('<i',0x401180-(0x401000+len(prefix)+5))+bytes.fromhex('c9c3');raw[0x200:0x200+len(owner)]=owner
inner=bytes.fromhex('558bec6aff684012400064a100000000506489250000000083ec048965f0c745fc00000000bbbc0a0000beef0d0000bf89070000ff150c204000c9c3');raw[0x300:0x300+len(inner)]=inner
middle_prefix=bytes.fromhex('535657bb11111111be22222222bf33333333')
middle=middle_prefix+b'\xe8'+struct.pack('<i',0x401100-(0x401180+len(middle_prefix)+5))+bytes.fromhex('5f5e5bc3');raw[0x380:0x380+len(middle)]=middle
raw[0x440:0x44a]=bytes.fromhex('b800154000e906000000');raw[0x450]=0xc3
struct.pack_into('<5I',raw,0x700,0x19930520,1,0x401520,0,0)
struct.pack_into('<2I',raw,0x720,0xffffffff,0x401380)
raw[0x400:0x40a]=bytes.fromhex('b830204000e906000000')
raw[0x410]=0xc3
raw[0x500:0x507]=bytes.fromhex('ff1510204000c3')
raw[0x550:0x55a]=bytes.fromhex('b800144000c645e800c3')
inner_cleanup=bytes.fromhex('0fb605d02040006bc00a83c001a2d0204000c3');raw[0x580:0x580+len(inner_cleanup)]=inner_cleanup
outer_cleanup=bytes.fromhex('0fb605d02040006bc00a83c002a2d0204000c3');raw[0x5a0:0x5a0+len(outer_cleanup)]=outer_cleanup
continuation=bytes.fromhex('8b4df464890d00000000b82a0000008be55dc3');raw[0x600:0x600+len(continuation)]=continuation
struct.pack_into('<5I',raw,0x830,0x19930520,2,0x402050,2,0x402060)
struct.pack_into('<4I',raw,0x850,0xffffffff,0,0,0x4013a0)
struct.pack_into('<5I',raw,0x860,0,0,1,1,0x402088)
struct.pack_into('<5I',raw,0x874,0,1,2,1,0x402098)
struct.pack_into('<4I',raw,0x888,0,0,0,0x401300)
struct.pack_into('<4I',raw,0x898,0,0,0,0x401350)
struct.pack_into('<4I',raw,0x8b0,0,0,0,0x4020c0)
struct.pack_into('<I',raw,0x8c0,0)
exe=out/'fixture.pe';exe.write_bytes(raw);image=out/'fixture-image.bin';image.write_bytes(pefile.PE(data=bytes(raw)).get_memory_mapped_image().ljust(0x3000,b"\0"))
m=json.loads((fixture/'manifest.json').read_text());m['sha256']=hashlib.sha256(raw).hexdigest();m['functions']={'startup':'0x401000','inner':'0x401100','middle':'0x401180','cleanup':'0x401380','cleanup_outer':'0x4013a0'}
manifest=out/'manifest.json';manifest.write_text(json.dumps(m))
repo=pathlib.Path(__file__).resolve().parents[2];lifted=out/'lifted'
subprocess.run([sys.executable,str(repo/'static-recomp/tools/lift_selected.py'),'--h5',str(h5),'--exe',str(exe),'--manifest',str(manifest),'--output',str(lifted),'--synthetic','--eh'],check=True)
code=(lifted/'lifted.c').read_text();start=code.index('void sub_00401000(void) {');end=code.index('void sub_',start+5);owner=code[start:end]
assert 'RECOMP_EH_ENTER()' in owner,owner
assert 'L_00401400:' in owner,owner
inner_start=code.index('void sub_00401100(void) {');inner_end=code.index('void sub_',inner_start+5);inner_body=code[inner_start:inner_end];assert 'recomp_eh_push(&_ehf)' in inner_body and '_ehf.resume=0xffffffffu' in inner_body,inner_body
assert 'case 0x401300u:' in code and 'case 0x401350u:' in code,code
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
flags=['-O1','-DRECOMP_GENERATED_CODE','-DRECOMP_TRACE','-DH4_BOOTSTRAP_RUNTIME','-include',str(lifted/'runtime_config.h'),'-I'+str(h5/'android/runtime'),'-I'+str(repo/'static-recomp/native')]
subprocess.run(['clang',*flags,str(repo/'static-recomp/native/poc_runtime.c'),str(repo/'static-recomp/tests/eh_landing_host.c'),str(repo/'static-recomp/native/eh_runtime.c'),str(dispatch),*[str(out/f'unit-{i}.o') for i in range(len(matches))],'-lm','-o',str(out/'landing-test')],check=True)
subprocess.run([str(out/'landing-test'),str(image)],check=True)
r=subprocess.run([str(out/'landing-test'),str(image)],capture_output=True,text=True,env=dict(os.environ,H4_EH_BAD_RESUME='1'))
assert r.returncode==78 and 'EH continuation not owned by landing frame' in r.stderr,(r.returncode,r.stderr)
# The real bootstrap must fail closed until its EH dispatcher is implemented.
r=subprocess.run([sys.executable,str(repo/'static-recomp/tools/build_bootstrap.py'),'--h5',str(h5),'--lifted',str(lifted),'--output',str(out/'blocked-startup')],text=True,capture_output=True)
assert r.returncode and 'EH-capable runtime' in r.stderr,(r.returncode,r.stderr)
assert not (out/'blocked-startup').exists()
print('PASS synthetic MSVC6 EH owner, catch dispatch, cold landing pad and split-compatible C compilation')
