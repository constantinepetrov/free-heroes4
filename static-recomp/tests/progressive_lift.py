"""Our own PE calls two own helpers, found through the bounded metadata catalog."""
import hashlib,json,os,pathlib,struct,subprocess,sys
import pefile
fixture,h5,output=map(pathlib.Path,sys.argv[1:]);output.mkdir(parents=True,exist_ok=True)
raw=bytearray((fixture/'synthetic.pe').read_bytes());marker=bytes.fromhex('ff1510204000c3');position=raw.index(marker)+len(marker)-1
call_va=0x401000+position-0x200;raw[position:position+6]=b'\xe8'+struct.pack('<i',0x401550-call_va-5)+b'\xc3'
raw[0x750:0x756]=b'\xe8'+struct.pack('<i',0x401560-0x401555)+b'\xc3';raw[0x760:0x766]=bytes.fromhex('b82a000000c3')
exe=output/'progressive.pe';exe.write_bytes(raw);image=output/'image.bin';image.write_bytes(pefile.PE(data=bytes(raw)).get_memory_mapped_image())
m=json.loads((fixture/'manifest.json').read_text());m['functions']={'startup':'0x401500'};m['sha256']=hashlib.sha256(raw).hexdigest();manifest=output/'input.json';manifest.write_text(json.dumps(m))
catalog=output/'catalog.json';catalog.write_text(json.dumps({'image_base':0x400000,'functions':[{'address':0x401550,'calls_to':[0x401560]},{'address':0x401560,'calls_to':[]}]}))
r=pathlib.Path(__file__).resolve().parents[2]
subprocess.run([sys.executable,str(r/'static-recomp/tools/advance_startup.py'),'--h5',str(h5),'--exe',str(exe),'--manifest',str(manifest),'--image',str(image),'--work',str(output/'work'),'--catalog',str(catalog),'--steps','3'],check=True)
log=(output/'work/last-startup.txt').read_text();assert 'original startup returned eax=0000002a' in log,log;assert 'entries=3 unique=3' in log,log
assert hashlib.sha256(exe.read_bytes()).hexdigest()==m['sha256'];print('PASS bounded progressive lifting and two actual synthetic guest helpers')
