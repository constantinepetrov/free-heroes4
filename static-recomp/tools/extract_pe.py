"""Extract only the reviewed game-root PE members from a self-extracting XZ/TAR."""
import argparse,hashlib,json,lzma,pathlib,tarfile
ap=argparse.ArgumentParser();ap.add_argument("installer",type=pathlib.Path);ap.add_argument("output",type=pathlib.Path);args=ap.parse_args()
repo=pathlib.Path(__file__).resolve().parents[2];out=args.output.resolve()
if out==repo or repo in out.parents:ap.error("Commercial input must stay outside Git")
allowed={"heroes4.exe","mss32.dll","binkw32.dll","mp3dec.asi","ddraw.dll"}
prefix="Heroes of Might and Magic IV/game/prefix/drive_c/game/";rows=[];out.mkdir(parents=True,exist_ok=True)
with args.installer.open("rb") as source:
    source.seek(1328763)
    if source.read(6)!=b"\xfd7zXZ\x00":ap.error("Unexpected installer layout; no shell code was run")
    source.seek(1328763)
    with lzma.LZMAFile(source) as xz,tarfile.open(fileobj=xz,mode="r|") as archive:
        for member in archive:
            if not member.isfile() or not member.name.startswith(prefix):continue
            rel=pathlib.PurePosixPath(member.name[len(prefix):])
            if len(rel.parts)!=1 or rel.name.lower() not in allowed:continue
            if rel.is_absolute() or ".." in rel.parts or not 0<member.size<64*1024**2:raise ValueError("Unsafe PE member")
            digest=hashlib.sha256();dest=out/rel.name
            with archive.extractfile(member) as inp,dest.open("xb") as target:
                while block:=inp.read(1024*1024):target.write(block);digest.update(block)
            if dest.stat().st_size!=member.size:raise ValueError("Truncated archive member")
            rows.append({"file":dest.name,"bytes":member.size,"sha256":digest.hexdigest()})
(out/"manifest.json").write_text(json.dumps(rows,indent=2)+"\n");print("Extracted",len(rows),"members; installer opened read-only")
