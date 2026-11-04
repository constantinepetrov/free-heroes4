"""Differential oracle: original x86 only in Unicorn; tested native program runs ARM64 C."""
import argparse,hashlib,json,os,pathlib,random,struct,subprocess,sys
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_EFLAGS

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--h5",type=pathlib.Path,required=True);ap.add_argument("--exe",type=pathlib.Path,required=True)
    ap.add_argument("--manifest",type=pathlib.Path,required=True);ap.add_argument("--native",type=pathlib.Path,required=True)
    ap.add_argument("--result",type=pathlib.Path,required=True);args=ap.parse_args()
    m=json.loads(args.manifest.read_text());repo=pathlib.Path(__file__).resolve().parents[2]
    if args.result.resolve()==repo or repo in args.result.resolve().parents:ap.error("Result must stay outside Git")
    if os.environ.get("GITHUB_ACTIONS")=="true" and not m.get("synthetic"):ap.error("Only synthetic input is allowed in CI")
    if hashlib.sha256(args.exe.read_bytes()).hexdigest()!=m["sha256"]:ap.error("PE hash mismatch")
    sys.path.insert(0,str(args.h5/"reference/pcrecomp"));from tools.pe.pe_analyze import analyze_pe,build_iat_map
    from tools.disasm.disasm32 import Disassembler
    info=analyze_pe(str(args.exe));dis=Disassembler(args.exe.read_bytes(),info.image_base,info.sections)
    allowed=set()
    for addr in m["functions"].values():
        fn=dis.disassemble_function(int(addr,0),build_iat_map(info))
        allowed.update(i.address for b in fn.blocks.values() for i in b.instructions)
    pe=pefile.PE(str(args.exe));u=Uc(UC_ARCH_X86,UC_MODE_32);u.mem_map(0x400000,0xe00000)
    u.mem_write(0x400000,pe.get_memory_mapped_image());u.mem_map(0xf0000000,0x1000);u.mem_map(0x2000000,0x1000)
    for name,to in [("get_error",0xf0000000),("get_tls",0xf0000004),("set_error",0xf0000008)]:u.mem_write(int(m["iat"][name],0),struct.pack("<I",to))
    u.mem_write(int(m["iat"]["tls_index"],0),bytes(4));last_error=0
    def word(at):return struct.unpack("<I",u.mem_read(at,4))[0]
    def write(at,value):u.mem_write(at,struct.pack("<I",value))
    def hook(machine,addr,size,unused):
        nonlocal last_error
        if addr in [0xf0000000,0xf0000004,0xf0000008]:
            sp=u.reg_read(UC_X86_REG_ESP);ret=word(sp);args=0
            if addr==0xf0000000:u.reg_write(UC_X86_REG_EAX,last_error)
            elif addr==0xf0000004:
                if word(sp+4)!=0:raise AssertionError("Unexpected TLS index")
                u.reg_write(UC_X86_REG_EAX,0x1001000);args=4
            else:last_error=word(sp+4);args=4
            u.reg_write(UC_X86_REG_ESP,sp+4+args);u.reg_write(UC_X86_REG_EIP,ret)
        elif addr not in allowed:raise AssertionError(f"x86 left the selected functions: {addr:#x}")
    u.hook_add(UC_HOOK_CODE,hook)
    gen=random.Random(0x4845524f);seeds=[0,1,2,0x7fffffff,0x80000000,0xffffffff]+[gen.getrandbits(32) for _ in range(1000)]
    counts={}
    for mode in ["rng","crt_rand","shuffle"]+(["x87"] if "x87" in m["functions"] else []):
        vectors=[(seed,1) for seed in seeds] if mode in ["rng","crt_rand"] else [(seed,count) for count in [1,2,3,7,16,64] for seed in seeds[:26]]
        run=subprocess.run([str(args.native.resolve()),mode],input="".join(f"{seed} {n}\n" for seed,n in vectors),capture_output=True,text=True,timeout=30)
        if run.returncode:raise AssertionError((mode,run.returncode,run.stderr))
        outputs=[tuple(map(int,line.split())) for line in run.stdout.splitlines()]
        if len(outputs)!=len(vectors):raise AssertionError("Missing native results")
        for (seed,n),native in zip(vectors,outputs):
            for reg,value in [(UC_X86_REG_EAX,0),(UC_X86_REG_ECX,0),(UC_X86_REG_EDX,0),(UC_X86_REG_EBX,0x11223344),(UC_X86_REG_ESI,0x22334455),(UC_X86_REG_EDI,0x33445566),(UC_X86_REG_EBP,0x44556677),(UC_X86_REG_ESP,0x1010000),(UC_X86_REG_EFLAGS,2)]:u.reg_write(reg,value)
            write(0x1010000,0x2000000);write(0x1001000,seed);write(0x1001014,seed);last_error=0x12345678
            if mode=="rng":u.reg_write(UC_X86_REG_ECX,0x1001000)
            if mode=="shuffle":
                for at,value in [(0x1010004,0x1002000),(0x1010008,0x1002000+4*n),(0x101000c,0x1001000)]:write(at,value)
                for i in range(n):write(0x1002000+4*i,0x101+i*17)
            if mode=="x87":
                write(0x1010004,seed);write(0x1010008,n);write(0x101000c,0)
            u.emu_start(int(m["functions"][mode],0),0x2000000,count=100000)
            reference=(u.reg_read(UC_X86_REG_EAX),word(0x1001014 if mode=="crt_rand" else 0x1001000))
            if mode=="shuffle":reference+=tuple(word(0x1002000+4*i) for i in range(n))
            if native!=reference:raise AssertionError((mode,seed,n,native,reference))
            if u.reg_read(UC_X86_REG_ESP)!=0x1010004 or last_error!=0x12345678:raise AssertionError("x86 ABI mismatch")
        counts[mode]=len(vectors)
    result={"sha256":m["sha256"],"synthetic":m.get("synthetic",False),"native_machine":subprocess.check_output(["uname","-m"],text=True).strip(),"platform":sys.platform,"passed":counts,"total":sum(counts.values()),"oracle":"input x86 instructions in bounded Unicorn","limitations":["preallocated TLS only","selected functions only; no WinMain/menu/scenario"]}
    args.result.write_text(json.dumps(result,indent=2)+"\n");print(json.dumps(result))
if __name__=="__main__":main()
