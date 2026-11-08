"""Run only our fixture and semantic host tests; reject false success on unsupported work."""
import subprocess,sys,os,pathlib,tempfile,struct
native,image=sys.argv[1:]
cases= [(['--bink-summary-live-handle-test'],77,'BinkGetSummary requires a decoder for non-NULL handle'),(['--bink-summary-null-output-test'],78,'BinkGetSummary NULL output buffer'),(['--dispatch-probe','0x401000'],0,'dispatch probe 00401000: FOUND'),(['--dispatch-probe','0x7b7980'],77,'dispatch probe 007b7980: MISSING'),(['--dispatch-probe','not-a-hex-address'],2,'invalid dispatch probe address'),(['--eh-binding-unsupported-test'],78,'unsupported EH catch object layout'),(['--eh-metadata-limit-test'],78,'EH metadata range exceeds limit'),(['--eh-dead-frame-test'],78,'EH crossed frame has no live landing pad'),(['--eh-binding-reference-test'],0,'PASS EH dispatcher nested rethrow'),(['--eh-chain-cycle-test'],78,'SEH chain cycle'),(['--eh-no-handler-test'],78,'EH catch not found'),(['--eh-malformed-test'],78,'unsupported SEH handler stub'),(['--eh-binding-test'],0,'PASS EH dispatcher nested rethrow'),(['--eh-cross-frame-test'],0,'PASS EH cross-frame search'),(['--eh-rethrow-outside'],78,'EH rethrow outside catch'),(['--eh-dispatch-test'],0,'PASS EH dispatcher nested rethrow'),(['--eh-state-at-max'],78,'invalid EH unwind state'),(['--eh-target-at-max'],78,'invalid EH unwind state'),(['--eh-invalid-unwind'],78,'invalid EH unwind transition'),(['--seh-trace-test'],0,'SEH chain cycle'),(['--selftest'],0,'PASS guest heap'),([image],0,'original startup returned'),(['--registry-read-test'],0,'PASS registry persistence'),(['--unknown-api'],77,'unimplemented Win32 import'),(['--blocking-wait'],77,'unsupported blocking wait')]
with tempfile.TemporaryDirectory(prefix='h4-host-tests-',dir=pathlib.Path(native).resolve().parent) as state:
 root=pathlib.Path(state)/'resources';(root/'data').mkdir(parents=True)
 (root/'data/Fixture.dat').write_bytes(bytes(range(32)));(root/'data/._Fixture.dat').write_bytes(b'host metadata')
 env=dict(os.environ,H4_STATE_DIR=state,H4_GAME_ROOT=str(root))
 for args,status,expected in cases:
  case_env=dict(env,H4_FRAME_OUTPUT=str(pathlib.Path(state)/"synthetic.bmp")) if args==["--selftest"] else env
  r=subprocess.run([native,*args],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30,env=case_env)
  assert r.returncode==status,(args,r.returncode,r.stdout)
  assert expected in r.stdout,r.stdout
  if args==[image] and pathlib.Path(image).name=='fixture-image.bin':assert 'EH invoking catch 00401300' in r.stdout and 'EH invoking catch 00401350' in r.stdout and 'EH continuing at 00401400' in r.stdout,r.stdout
  if args==['--eh-binding-unsupported-test']:assert 'EH cleanup' not in r.stdout,r.stdout
  if args==['--selftest']:
   assert 'PASS EH unwind cleanup order' in r.stdout,r.stdout
   assert 'PASS Bink disabled-video summary without fake decoder' in r.stdout,r.stdout
  print(r.stdout,end='')
 capture=(pathlib.Path(state)/'synthetic.bmp').read_bytes();assert capture[:2]==b'BM' and struct.unpack_from('<ii',capture,18)==(320,200)
 assert (root/'data/Fixture.dat').read_bytes()==bytes(range(32))
 assert not (root/'new.dat').exists()
 assert (pathlib.Path(state)/'files/new.dat').read_bytes()==bytes.fromhex('78563412')
print('PASS synthetic startup, unsupported API and blocking-wait diagnostics')
