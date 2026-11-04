"""Result paths must never overwrite the input PE."""
import hashlib,pathlib,subprocess,sys
fixture=pathlib.Path(sys.argv[1]);h5=pathlib.Path(sys.argv[2]);native=pathlib.Path(sys.argv[3]);root=pathlib.Path(__file__).resolve().parents[1]
pe=fixture/"synthetic.pe";before=hashlib.sha256(pe.read_bytes()).hexdigest()
commands=[
 [sys.executable,str(root/"tools/pe_audit.py"),str(fixture),str(pe)],
 [sys.executable,str(root/"tools/verify_selected.py"),"--h5",str(h5),"--exe",str(pe),"--manifest",str(fixture/"manifest.json"),"--native",str(native),"--result",str(pe)]
]
for cmd in commands:
    result=subprocess.run(cmd,capture_output=True,text=True,timeout=5)
    assert result.returncode==2 and "overwrite an input" in result.stderr,(cmd,result.returncode,result.stderr)
    assert hashlib.sha256(pe.read_bytes()).hexdigest()==before
print("PASS input PE cannot be overwritten by an audit or result path")
