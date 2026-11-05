"""Run only our fixture and semantic host tests; reject false success on unsupported work."""
import subprocess,sys,os,pathlib,tempfile,struct
native,image=sys.argv[1:]
cases= [(['--seh-trace-test'],0,'SEH chain cycle'),(['--selftest'],0,'PASS guest heap'),([image],0,'original startup returned'),(['--registry-read-test'],0,'PASS registry persistence'),(['--unknown-api'],77,'unimplemented Win32 import'),(['--blocking-wait'],77,'unsupported blocking wait')]
with tempfile.TemporaryDirectory(prefix='h4-host-tests-',dir=pathlib.Path(native).resolve().parent) as state:
 root=pathlib.Path(state)/'resources';(root/'data').mkdir(parents=True)
 (root/'data/Fixture.dat').write_bytes(bytes(range(32)));(root/'data/._Fixture.dat').write_bytes(b'host metadata')
 env=dict(os.environ,H4_STATE_DIR=state,H4_GAME_ROOT=str(root))
 for args,status,expected in cases:
  case_env=dict(env,H4_FRAME_OUTPUT=str(pathlib.Path(state)/"synthetic.bmp")) if args==["--selftest"] else env
  r=subprocess.run([native,*args],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=case_env)
  assert r.returncode==status,(args,r.returncode,r.stdout)
  assert expected in r.stdout,r.stdout
  print(r.stdout,end='')
 capture=(pathlib.Path(state)/'synthetic.bmp').read_bytes();assert capture[:2]==b'BM' and struct.unpack_from('<ii',capture,18)==(320,200)
 assert (root/'data/Fixture.dat').read_bytes()==bytes(range(32))
 assert not (root/'new.dat').exists()
 assert (pathlib.Path(state)/'files/new.dat').read_bytes()==bytes.fromhex('78563412')
print('PASS synthetic startup, unsupported API and blocking-wait diagnostics')
