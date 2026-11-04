"""Run only our fixture and semantic host tests; reject false success on unsupported work."""
import subprocess,sys,os,pathlib,tempfile
native,image=sys.argv[1:]
cases= [(['--selftest'],0,'PASS guest heap'),([image],0,'original startup returned'),(['--registry-read-test'],0,'PASS registry persistence'),(['--unknown-api'],77,'unimplemented Win32 import'),(['--blocking-wait'],77,'unsupported blocking wait')]
with tempfile.TemporaryDirectory(prefix='h4-host-tests-',dir=pathlib.Path(native).resolve().parent) as state:
 env=dict(os.environ,H4_STATE_DIR=state)
 for args,status,expected in cases:
  r=subprocess.run([native,*args],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=env)
  assert r.returncode==status,(args,r.returncode,r.stdout)
  assert expected in r.stdout,r.stdout
  print(r.stdout,end='')
print('PASS synthetic startup, unsupported API and blocking-wait diagnostics')
