"""Own PE32 fixture. Instruction bytes below are authored for this test, not from Heroes IV."""
import hashlib,json,pathlib,struct,sys
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
base=0x400000
# Own LCG differs from the game: Numerical Recipes constants, full 32-bit result.
rng=bytes.fromhex("8b01 69c00d661900 055ff36e3c 8901 c3")
crt=bytearray(bytes.fromhex("53 ff15"))+struct.pack("<I",0x402000)+bytes.fromhex("8bd8 6a00 ff15")+struct.pack("<I",0x402004)+bytes.fromhex("83c014 8bc8")
crt+=b"\xe8"+struct.pack("<i",0x401000-(0x401100+len(crt)+5))
crt+=bytes.fromhex("53 ff15")+struct.pack("<I",0x402008)+bytes.fromhex("5b c3")
# Own deterministic reverse-loop fixture, not the game's shuffle.
shuffle=bytearray.fromhex("56 57 8b74240c 8b7c2410 83ef04")
loop=len(shuffle);shuffle+=bytes.fromhex("39fe 73 00");branch=len(shuffle)-1
shuffle+=bytes.fromhex("8b06 8b17 8916 8907 83c604 83ef04 eb00")
shuffle[-1]=(loop-len(shuffle))&255;shuffle[branch]=(len(shuffle)-(branch+1))&255
shuffle+=bytes.fromhex("5f 5e c3")
thread_data=bytes.fromhex("b800100001 c3")
x87=bytes.fromhex("db442404 da742408 db5c240c 8b44240c c3")
text=bytearray(b"\xcc"*0x600)
for off,code in [(0,rng),(0x100,crt),(0x200,thread_data),(0x300,shuffle),(0x400,x87)]:text[off:off+len(code)]=code
pe=bytearray(0x200);pe[:2]=b"MZ";struct.pack_into("<I",pe,0x3c,0x80);pe[0x80:0x84]=b"PE\0\0"
struct.pack_into("<HHIIIHH",pe,0x84,0x14c,1,0,0,0,224,0x102)
o=0x98;struct.pack_into("<H",pe,o,0x10b);struct.pack_into("<III",pe,o+4,len(text),0,0)
struct.pack_into("<III",pe,o+16,0x1000,0x1000,0x2000);struct.pack_into("<III",pe,o+28,base,0x1000,0x200)
struct.pack_into("<HH",pe,o+40,4,0);struct.pack_into("<HH",pe,o+48,4,0);struct.pack_into("<II",pe,o+56,0x3000,0x200)
struct.pack_into("<HH",pe,o+68,3,0);struct.pack_into("<IIIII",pe,o+72,0x100000,0x1000,0x100000,0x1000,0);struct.pack_into("<I",pe,o+92,16)
s=o+224;pe[s:s+8]=b".text\0\0\0";struct.pack_into("<IIII",pe,s+8,len(text),0x1000,len(text),0x200);struct.pack_into("<I",pe,s+36,0x60000020)
raw=bytes(pe+text);(out/"synthetic.pe").write_bytes(raw)
m={"sha256":hashlib.sha256(raw).hexdigest(),"synthetic":True,"functions":{"rng":"0x401000","crt_rand":"0x401100","thread_data":"0x401200","shuffle":"0x401300","x87":"0x401400"},"iat":{"get_error":"0x402000","get_tls":"0x402004","set_error":"0x402008","tls_index":"0x40200c"}}
(out/"manifest.json").write_text(json.dumps(m,indent=2)+"\n");print("Own synthetic PE32:",m["sha256"])
