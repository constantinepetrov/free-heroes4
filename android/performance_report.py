import json,pathlib,sys
root=pathlib.Path(sys.argv[1]);all_results={}
for p in sorted(root.glob('*-test.txt')):
    line=next(s for s in p.read_text().splitlines() if 'BENCHMARK [' in s)
    all_results[p.stem.replace('-test','')]=json.loads(line.split('BENCHMARK ',1)[1])
(root/'results.json').write_text(json.dumps(all_results,indent=2))
rows=['| Build | Scenario | FPS | Median ms | p95 ms | p99 ms | CPU % | Peak RSS KiB |','|---|---|---:|---:|---:|---:|---:|---:|']
for build,cases in all_results.items():
    for c in cases:
        t=c['timing'];rows.append(f"| {build} | {c['scenario']} | {t['fps']} | {t['median_ms']} | {t['p95_ms']} | {t['p99_ms']} | {c['cpu_percent']} | {c['peak_rss_kib']} |")
(root/'results.md').write_text('\n'.join(rows)+'\n');print('\n'.join(rows))
