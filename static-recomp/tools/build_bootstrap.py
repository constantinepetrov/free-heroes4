"""Build a private native startup host; generated code stays outside the checkout."""
import argparse, pathlib, subprocess
p=argparse.ArgumentParser();p.add_argument('--h5',type=pathlib.Path,required=True);p.add_argument('--lifted',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
r=pathlib.Path(__file__).resolve().parents[2];out=a.output.resolve()
if out==r or r in out.parents:p.error('Build output must stay outside Git')
subprocess.run(['clang','-O1','-g','-fwrapv','-fno-strict-aliasing','-ffp-contract=off','-DRECOMP_GENERATED_CODE','-DRECOMP_TRACE','-DH4_BOOTSTRAP_RUNTIME',f'-DH4_DLMALLOC_SOURCE="{a.h5.resolve()}/reference/dlmalloc/malloc.c"','-include',str(a.lifted/'runtime_config.h'),'-I'+str(a.h5/'android/runtime'),'-I'+str(r/'static-recomp/native'),str(a.lifted/'lifted.c'),*[str(r/'static-recomp/native'/s) for s in ['poc_runtime.c','guest_memory.c','guest_strings.c','bootstrap.c']],'-pthread','-lm','-o',str(out)],check=True)
