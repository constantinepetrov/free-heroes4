"""Lift explicitly selected PE32 functions; generated game code must stay outside Git."""
import argparse, hashlib, importlib, json, os, pathlib, subprocess, sys
H5_COMMIT = "6dc7914d7529d92beaf323c0f88a5102b4d0fd69"

def main():
    ap=argparse.ArgumentParser();ap.add_argument("--h5",type=pathlib.Path,required=True)
    ap.add_argument("--exe",type=pathlib.Path,required=True);ap.add_argument("--manifest",type=pathlib.Path,required=True)
    ap.add_argument("--output",type=pathlib.Path,required=True);ap.add_argument("--synthetic",action="store_true");ap.add_argument("--eh",action="store_true",help="experimental EH landing pads; requires an EH-capable host")
    args=ap.parse_args();repo=pathlib.Path(__file__).resolve().parents[2];out=args.output.resolve()
    if {args.exe.resolve(),args.manifest.resolve()} & {out/"lifted.c",out/"runtime_config.h",out/"lift-summary.json"}:ap.error("Generated output would overwrite an input file")
    if out==repo or repo in out.parents:ap.error("Output must be outside this Git checkout")
    if os.environ.get("GITHUB_ACTIONS")=="true" and not args.synthetic:ap.error("CI accepts synthetic input only")
    revision=subprocess.check_output(["git","-C",str(args.h5),"rev-parse","HEAD"],text=True).strip()
    if revision!=H5_COMMIT:ap.error("Unexpected H5 tool revision")
    manifest=json.loads(args.manifest.read_text());raw=args.exe.read_bytes()
    digest=hashlib.sha256(raw).hexdigest()
    if digest!=manifest["sha256"]:ap.error("PE hash differs from the reviewed manifest")
    if args.synthetic and not manifest.get("synthetic"):ap.error("Not a synthetic fixture")
    sys.path.insert(0,str(args.h5/"tools"));from lift_game import CheckedLifter,lift_function_linear,complete_eh_owner
    from tools.pe.pe_analyze import analyze_pe,build_iat_map
    from tools.disasm.disasm32 import Disassembler
    from tools.lift.eh32 import funcinfo,catch_funclets,PROLOGUE_WINDOW
    info=analyze_pe(str(args.exe))
    if info.machine!="i386" or info.pe_type!="PE32":ap.error("Only reviewed PE32 i386 inputs are supported")
    iat=build_iat_map(info);dis=Disassembler(raw,info.image_base,info.sections)
    named={k:int(v,0) for k,v in manifest["functions"].items()};addresses=set(named.values())
    functions={}
    pending=set(addresses)
    while pending:
        va=min(pending);pending.remove(va)
        fn=dis.disassemble_function(va,iat)
        if fn is None:raise ValueError(f"No decoded function at {va:#x}")
        if args.eh:
            complete_eh_owner(dis,fn,iat)
            funclets=set()
            if fn.eh_resume is not None:
                handler=None
                ins=sorted({i.address:i for b in fn.blocks.values() for i in b.instructions}.values(),key=lambda i:i.address)
                for instruction in ins[:PROLOGUE_WINDOW]:
                    if instruction.mnemonic=="push" and instruction.op_str.startswith("0x"):
                        handler=int(instruction.op_str,16)
                    elif instruction.mnemonic=="mov" and instruction.op_str.endswith("dword ptr fs:[0]") and handler is not None:
                        fi=funcinfo(dis.read_bytes,handler)
                        if fi is None:raise ValueError(f"EH owner {va:#x} lacks verified FuncInfo")
                        funclets.update(catch_funclets(dis.read_bytes,fi));break
                else:raise ValueError(f"EH owner {va:#x} lacks a handler registration")
            # A catch may write memory after loading its continuation into EAX.
            # Accept only a straight-line tail of memory stores; stop on anything else.
            for entry in sorted(funclets):
                catch=dis.disassemble_function(entry,iat)
                if catch is None:raise ValueError(f"Cannot decode EH catch {entry:#x}")
                for block in catch.blocks.values():
                    seq=block.instructions
                    if not seq or not seq[-1].is_ret:continue
                    for instruction in reversed(seq[:-1]):
                        if instruction.mnemonic=="mov" and instruction.op_str.startswith("eax, 0x"):
                            resume=int(instruction.op_str[5:],16)
                            if not dis.is_code_address(resume):raise ValueError("EH continuation outside code")
                            if resume not in {i.address for b in fn.blocks.values() for i in b.instructions}:
                                cold=dis.disassemble_function(resume,iat)
                                if cold is None or cold.num_instructions>16384:raise ValueError("Cannot decode EH continuation")
                                fn.blocks.update(cold.blocks);fn.calls_to.update(cold.calls_to);fn.jump_targets.update(cold.jump_targets)
                            fn.eh_resume.add(resume);break
                        if instruction.mnemonic=="mov" and instruction.op_str.startswith(("byte ptr [","word ptr [","dword ptr [")):
                            continue
                        break
            for target in funclets:
                if target not in addresses and dis.is_code_address(target):
                    if len(addresses)>=10000:raise ValueError("EH function set exceeds the POC bound")
                    addresses.add(target);pending.add(target)
        functions[va]=fn
    lifter=CheckedLifter(iat_map=iat,lifted=addresses,precise_sbb=True,precise_carry=True)
    parts=['#include "recomp_types.h"']+[f'void sub_{va:08X}(void);' for va in sorted(addresses)];counts={}
    for va in sorted(addresses):
        fn=functions[va]
        ins=sorted({i.address:i for b in fn.blocks.values() for i in b.instructions}.values(),key=lambda i:i.address)
        if len(ins)>10000:raise ValueError("Selected function exceeds the POC bound")
        body=lift_function_linear(lifter,f"sub_{va:08X}",ins,set(fn.blocks),va,**({"indirect_targets":fn.jump_targets,"eh_resume":fn.eh_resume} if args.eh else {}))
        if args.eh and fn.eh_resume is not None and not fn.eh_resume:
            # A cleanup-only guest frame still needs a live register snapshot.
            marker=f"RECOMP_ENTER(0x{va:08X}u);"
            registration="recomp_ehframe_t _ehf; _ehf.entry_esp=g_esp; recomp_eh_push(&_ehf); _ehf.resume=0xffffffffu;"
            if fn.eh_frame_bias:registration+=f" _ehf.entry_esp -= 0x{fn.eh_frame_bias:X}u;"
            assert body.count(marker)==1
            body=body.replace(marker,marker+"\n    "+registration)
            body=body.replace('RECOMP_REGS_OUT(); return;', 'RECOMP_EH_LEAVE(); RECOMP_REGS_OUT(); return;')
            assert body.endswith('    RECOMP_REGS_OUT();\n}')
            body=body[:-len('    RECOMP_REGS_OUT();\n}')]+ '    RECOMP_EH_LEAVE(); RECOMP_REGS_OUT();\n}'
        if args.eh and fn.eh_resume and fn.eh_frame_bias:
            assert body.count("    RECOMP_EH_ENTER();")==1
            body=body.replace("    RECOMP_EH_ENTER();", "    RECOMP_EH_ENTER();\n"+f"    _ehf.entry_esp -= 0x{fn.eh_frame_bias:X}u;")
        if "UNIMPLEMENTED" in body:raise ValueError("Unchecked unsupported instruction")
        body=body.replace(f"RECOMP_ENTER(0x{va:08X}u);",f"RECOMP_ENTER(0x{va:08X}u);\n#ifdef H4_BOOTSTRAP_RUNTIME\n    h4_record_entry(0x{va:08X}u);\n#endif")
        parts.append(body);counts[hex(va)]=len(ins)
    for name,va in named.items():
        if not name.isidentifier():raise ValueError("Invalid wrapper name")
        parts.append(f"void h4_{name}(void) {{ sub_{va:08X}(); }}")
    parts.append("void (*h4_find_lifted(uint32_t va))(void) { switch (va) {")
    for va in sorted(addresses):parts.append(f"case {va:#x}u: return sub_{va:08X};")
    parts.append("default: return 0; }}")
    parts.append("int h4_eh_resume_allowed(uint32_t owner,uint32_t va) { switch(owner) {")
    for va in sorted(addresses):
        resumes=functions[va].eh_resume if args.eh else None
        if resumes:
            tests=" || ".join(f"va == {target:#x}u" for target in sorted(resumes))
            parts.append(f"case {va:#x}u: return {tests};")
    parts.append("default: return 0; }}")
    out.mkdir(parents=True,exist_ok=True);(out/"lifted.c").write_text("\n".join(parts)+"\n")
    config=['int h4_eh_resume_allowed(unsigned int owner, unsigned int va);','void void_lift_trap(unsigned int va, const char *reason);','void h4_record_entry(unsigned int va);','void (*h4_find_lifted(unsigned int va))(void);','#include <stdint.h>','extern unsigned char *h4_guest_base;','#define VOID_GUEST_BASE ((uintptr_t)h4_guest_base)']
    for name,value in manifest["iat"].items():
        if not name.isidentifier():raise ValueError("Invalid IAT macro")
        config.append(f"#define H4_{name.upper()} {int(value,0):#x}u")
    config.append(f"#define H4_IMAGE_BASE {info.image_base:#x}u")
    import pefile
    config.append(f"#define H4_IMAGE_SIZE {pefile.PE(data=raw).OPTIONAL_HEADER.SizeOfImage:#x}u")
    for name,value in manifest.get("milestones",{}).items():
        if not name.isidentifier():raise ValueError("Invalid milestone")
        config.append(f"#define H4_MILESTONE_{name.upper()} {int(value,0):#x}u")
    if "startup" in named:
        config.append("#define H4_HAS_STARTUP 1")
        config.append("struct h4_import_name {uint32_t iat; const char* name;};")
        entries=["{%#xu, %s}"%(va,json.dumps(dll+"!"+symbol)) for va,(dll,symbol) in sorted(iat.items())]
        config.append("static const struct h4_import_name h4_import_names[] = {"+",".join(entries or ["{0,(const char*)0}"])+"};")
        config.append(f"#define H4_IMPORT_COUNT {len(entries)}u")
    if "x87" in named:config.append("#define H4_HAS_X87 1")
    for name in named:
        if not name.startswith("helper_"):config.append(f"void h4_{name}(void);")
    (out/"runtime_config.h").write_text("\n".join(config)+"\n")
    (out/"lift-summary.json").write_text(json.dumps({"sha256":digest,"h5_commit":revision,"functions":counts,"synthetic":args.synthetic},indent=2)+"\n")
    print(json.dumps({"functions":len(counts),"instructions":sum(counts.values()),"output":str(out),"synthetic":args.synthetic}))
if __name__=="__main__":main()
