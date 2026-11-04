"""Lift explicitly selected PE32 functions; generated game code must stay outside Git."""
import argparse, hashlib, importlib, json, os, pathlib, subprocess, sys
H5_COMMIT = "6dc7914d7529d92beaf323c0f88a5102b4d0fd69"

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--h5",type=pathlib.Path,required=True)
    ap.add_argument("--exe",type=pathlib.Path,required=True);ap.add_argument("--manifest",type=pathlib.Path,required=True)
    ap.add_argument("--output",type=pathlib.Path,required=True);ap.add_argument("--synthetic",action="store_true")
    args=ap.parse_args();repo=pathlib.Path(__file__).resolve().parents[2];out=args.output.resolve()
    if out==repo or repo in out.parents:ap.error("Output must be outside this Git checkout")
    if os.environ.get("GITHUB_ACTIONS")=="true" and not args.synthetic:ap.error("CI accepts synthetic input only")
    revision=subprocess.check_output(["git","-C",str(args.h5),"rev-parse","HEAD"],text=True).strip()
    if revision!=H5_COMMIT:ap.error("Unexpected H5 tool revision")
    manifest=json.loads(args.manifest.read_text());raw=args.exe.read_bytes()
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=manifest["sha256"]:ap.error("PE hash differs from the reviewed manifest")
    if args.synthetic and not manifest.get("synthetic"):ap.error("Not a synthetic fixture")
    sys.path.insert(0,str(args.h5/"tools"));from lift_game import CheckedLifter,lift_function_linear
    from tools.pe.pe_analyze import analyze_pe,build_iat_map
    from tools.disasm.disasm32 import Disassembler
    info=analyze_pe(str(args.exe))
    if info.machine!="i386" or info.pe_type!="PE32":ap.error("Only reviewed PE32 i386 inputs are supported")
    iat=build_iat_map(info);dis=Disassembler(raw,info.image_base,info.sections)
    named={k:int(v,0) for k,v in manifest["functions"].items()};addresses=set(named.values())
    lifter=CheckedLifter(iat_map=iat,lifted=addresses,precise_sbb=True,precise_carry=True)
    parts=['#include "recomp_types.h"']+[f'void sub_{va:08X}(void);' for va in sorted(addresses)];counts={}
    for va in sorted(addresses):
        fn=dis.disassemble_function(va,iat)
        if fn is None:raise ValueError(f"No decoded function at {va:#x}")
        ins=sorted({i.address:i for b in fn.blocks.values() for i in b.instructions}.values(),key=lambda i:i.address)
        if len(ins)>10000:raise ValueError("Selected function exceeds the POC bound")
        body=lift_function_linear(lifter,f"sub_{va:08X}",ins,set(fn.blocks),va)
        if "UNIMPLEMENTED" in body:raise ValueError("Unchecked unsupported instruction")
        parts.append(body);counts[hex(va)]=len(ins)
    for name,va in named.items():
        if not name.isidentifier():raise ValueError("Invalid wrapper name")
        parts.append(f"void h4_{name}(void) {{ sub_{va:08X}(); }}")
    out.mkdir(parents=True,exist_ok=True);(out/"lifted.c").write_text("\n".join(parts)+"\n")
    config=['#include <stdint.h>','extern unsigned char *h4_guest_base;','#define VOID_GUEST_BASE ((uintptr_t)h4_guest_base)']
    for name,value in manifest["iat"].items():
        if not name.isidentifier():raise ValueError("Invalid IAT macro")
        config.append(f"#define H4_{name.upper()} {int(value,0):#x}u")
    if "startup" in named:
        config.append("#define H4_HAS_STARTUP 1")
        config.append("struct h4_import_name {uint32_t iat; const char* name;};")
        entries=["{%#xu, %s}"%(va,json.dumps(dll+"!"+symbol)) for va,(dll,symbol) in sorted(iat.items())]
        config.append("static const struct h4_import_name h4_import_names[] = {"+",".join(entries or ["{0,(const char*)0}"])+"};")
        config.append(f"#define H4_IMPORT_COUNT {len(entries)}u")
    if "x87" in named:config.append("#define H4_HAS_X87 1")
    for name in named:config.append(f"void h4_{name}(void);")
    (out/"runtime_config.h").write_text("\n".join(config)+"\n")
    (out/"lift-summary.json").write_text(json.dumps({"sha256":digest,"h5_commit":revision,"functions":counts,"synthetic":args.synthetic},indent=2)+"\n")
    print(json.dumps({"functions":len(counts),"instructions":sum(counts.values()),"output":str(out),"synthetic":args.synthetic}))
if __name__=="__main__":main()
