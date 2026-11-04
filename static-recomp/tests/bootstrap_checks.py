"""Run only our fixture and semantic host tests; reject false success on unsupported work."""
import subprocess,sys
native,image=sys.argv[1:]
for args,status,expected in [(['--selftest'],0,'PASS guest heap'),([image],0,'original startup returned'),(['--unknown-api'],77,'unimplemented Win32 import'),(['--blocking-wait'],77,'unsupported blocking wait')]:
 r=subprocess.run([native,*args],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
 assert r.returncode==status,(args,r.returncode,r.stdout)
 assert expected in r.stdout,r.stdout
 print(r.stdout,end='')
print('PASS synthetic startup, unsupported API and blocking-wait diagnostics')
