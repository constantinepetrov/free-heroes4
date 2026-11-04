"""Read-only PE inventory. Output is metadata, never a copy of PE sections."""
import argparse,hashlib,json,pathlib
import pefile
ap=argparse.ArgumentParser();ap.add_argument("directory",type=pathlib.Path);ap.add_argument("output",type=pathlib.Path);args=ap.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2]
if args.output.resolve()==repo or repo in args.output.resolve().parents:ap.error("Keep private audit data outside Git")
if args.output.resolve() in {p.resolve() for p in args.directory.iterdir() if p.is_file()}:ap.error("Audit output would overwrite an input file")
rows=[]
for path in sorted(args.directory.iterdir()):
    if path.name.startswith("._") or path.suffix.lower() not in [".exe",".dll",".asi"]:continue
    raw=path.read_bytes();pe=pefile.PE(data=raw);versions={}
    for block in getattr(pe,"FileInfo",[]):
        for item in block:
            for table in getattr(item,"StringTable",[]):versions.update({k.decode("utf8","replace"):v.decode("utf8","replace") for k,v in table.entries.items()})
    imports={d.dll.decode("ascii","replace"):[i.name.decode("ascii","replace") if i.name else "#"+str(i.ordinal) for i in d.imports] for d in getattr(pe,"DIRECTORY_ENTRY_IMPORT",[])}
    rows.append({"file":path.name,"sha256":hashlib.sha256(raw).hexdigest(),"bytes":len(raw),"machine":hex(pe.FILE_HEADER.Machine),"magic":hex(pe.OPTIONAL_HEADER.Magic),"base":hex(pe.OPTIONAL_HEADER.ImageBase),"entry":hex(pe.OPTIONAL_HEADER.ImageBase+pe.OPTIONAL_HEADER.AddressOfEntryPoint),"versions":versions,"imports":imports,"tls_directory":hasattr(pe,"DIRECTORY_ENTRY_TLS"),"relocations":sum(len(b.entries) for b in getattr(pe,"DIRECTORY_ENTRY_BASERELOC",[])),"sections":[{"name":s.Name.rstrip(b"\0").decode("ascii","replace"),"raw_size":s.SizeOfRawData,"virtual_size":s.Misc_VirtualSize,"flags":hex(s.Characteristics),"entropy":round(s.get_entropy(),3)} for s in pe.sections]})
args.output.write_text(json.dumps(rows,ensure_ascii=False,indent=2)+"\n");print("Audited",len(rows),"PE files without executing them")
