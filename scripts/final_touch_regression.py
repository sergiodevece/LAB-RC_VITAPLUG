#!/usr/bin/env python3
"""Compare two actual local macOS Release processor builds; no host, network or installed AU."""
import argparse
from array import array
import csv
import hashlib
import json
import math
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
HARNESS = ROOT / 'Tests' / 'FinalTouchRegression.cpp'


def digest_file(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for buf in iter(lambda: stream.read(1024*1024), b''):
            h.update(buf)
    return h.hexdigest()


def link(build, dest):
    # Reuse that checkout's discovered JUCE flags and its completed Release shared code.
    # The historical build never reads a production header or object from the candidate.
    commands = json.loads((build / 'compile_commands.json').read_text())
    entry = next(c for c in commands if c['file'].endswith('/Tests/ProcessorTests.cpp'))
    obj = dest / 'FinalTouchRegression.o'
    executable = dest / 'FinalTouchRegression'
    cmd = shlex.split(entry['command'])
    cmd[cmd.index('-o')+1] = str(obj)
    cmd[cmd.index(entry['file'])] = str(HARNESS)
    subprocess.run(cmd, cwd=entry['directory'], check=True)
    cmd = shlex.split((build / 'CMakeFiles/labrc_tests.dir/link.txt').read_text())
    cmd = [a for a in cmd if not a.endswith('.cpp.o')]
    cmd.insert(cmd.index('-o'),str(obj))
    cmd[cmd.index('-o')+1] = str(executable)
    subprocess.run(cmd, cwd=build, check=True)
    return executable


def rows(dest):
    with (dest/'segments.tsv').open() as stream:
        return list(csv.DictReader(stream,delimiter='\t'))


def capture(args):
    build, dest, legacy = (path.resolve() for path in (args.build,args.out,args.legacy))
    dest.mkdir(exist_ok=True,parents=True)
    if any((dest/name).exists() for name in ('manifest.json','samples.f32','segments.tsv')):
        raise ValueError('Render destination must be fresh; choose another output directory')
    executable = link(build,dest)
    if args.make_legacy:
        if legacy.exists(): raise ValueError('Preserving existing legacy fixture')
        subprocess.run([executable,'legacy',legacy],check=True)
    if not legacy.is_file(): raise FileNotFoundError(legacy)
    subprocess.run([executable,'render',dest,legacy,args.mode],check=True)
    with (dest/'performance.tsv').open('w') as output:
        output.write('rate\tblock\tcallbacks\tmean_us\tp95_us\tp99_us\tmax_us\toverruns\trss_mib\n'); output.flush()
        for rate in (48000,96000,192000):
            for block in (64,256):
                subprocess.run([executable,'bench',str(rate),str(block),args.mode],stdout=output,check=True)
    segments=rows(dest); expected_offset=0; checks={}; switch={}
    if len(segments)!=6*(9*4+3): raise ValueError('Missing segments')
    with (dest/'samples.f32').open('rb') as audio:
        for row in segments:
            key=f"{row['rate']}/{row['name']}"
            if int(row['offset_bytes']) != expected_offset or int(row['latency'])!=128: raise ValueError(key)
            count=8*int(row['frames']); data=audio.read(count)
            if len(data)!=count: raise ValueError('Incomplete render '+key)
            expected_offset+=count; checks[key]=hashlib.sha256(data).hexdigest()
            if row['name']=='room-switch-0': switch[row['rate']]=checks[key]
            if row['name']=='room-switch-1' and checks[key]!=switch[row['rate']]:
                raise ValueError('Worker was not reproducible at '+row['rate']+'; retain output for diagnosis')
        if audio.read(1): raise ValueError('Trailing samples')
    manifest={'label':args.label,'mode':args.mode,'format':'stereo interleaved little-endian IEEE float32',
              'version_evidence':str(build),'sample_bytes':expected_offset,'segments':len(segments),
              'processor_harness_sha256':digest_file(HARNESS),'legacy_state_sha256':digest_file(legacy),
              'sample_sha256':digest_file(dest/'samples.f32'),'segment_sha256':checks}
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(json.dumps({k:v for k,v in manifest.items() if k!='segment_sha256'}))


def compare(args):
    a,b=args.reference,args.candidate
    ref,other=rows(a),rows(b)
    if ref!=other: raise ValueError('Layouts, order, rate, latency or duration differ')
    manifests=[json.loads((d/'manifest.json').read_text()) for d in (a,b)]
    for key in ('sample_bytes','segments','processor_harness_sha256','legacy_state_sha256'):
        if manifests[0][key]!=manifests[1][key]: raise ValueError('Incomparable '+key)
    failures=[];sum_sq=0.0;all_count=0;peak=0.;bit_differences=0
    with (a/'samples.f32').open('rb') as sa,(b/'samples.f32').open('rb') as sb:
        for row in ref:
            n=2*int(row['frames']);ba=sa.read(4*n);bb=sb.read(4*n);all_count+=n
            if len(ba)!=4*n or len(bb)!=4*n: raise ValueError('Truncated samples')
            if ba==bb: continue
            left,right=array('f'),array('f');left.frombytes(ba);right.frombytes(bb)
            count=sum(x!=y for x,y in zip(memoryview(ba).cast('I'),memoryview(bb).cast('I')))
            local=sum((x-y)**2 for x,y in zip(left,right)); p=max(abs(x-y) for x,y in zip(left,right));peak=max(peak,p);sum_sq+=local
            failures.append({'rate':row['rate'],'segment':row['name'],'changed_sample_bits':count,'peak':p,'rms':math.sqrt(local/n)})
            bit_differences+=count
        if sa.read(1) or sb.read(1):raise ValueError('Trailing samples')
    report={'reference':manifests[0]['label'],'candidate':manifests[1]['label'],'segments':len(ref),'float_samples':all_count,'bit_different_floats':bit_differences,'peak_abs_residual':peak,'rms_residual':math.sqrt(sum_sq/all_count),'different_segments':failures}
    args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='different_segments'}));return int(bit_differences!=0)


def paired_benchmark(args):
    """Rotate three independent processes to avoid sequential warming favouring one version."""
    modes = {'0.1': (args.reference, 'off'), '0.2-out': (args.candidate, 'off'),
             '0.2-flat': (args.candidate, 'flat')}
    names = list(modes)
    fields = 'rate block callbacks mean_us p95_us p99_us max_us overruns rss_mib'.split()
    with args.output.open('x', newline='') as target:
        writer = csv.DictWriter(target, fieldnames=['round', 'processor', *fields], delimiter='\t')
        writer.writeheader()
        for rate in (48000, 96000, 192000):
            for block in (64, 256):
                for turn in range(3):
                    order = names[turn:] + names[:turn]
                    for label in order:
                        folder, mode = modes[label]
                        result = subprocess.check_output([folder/'FinalTouchRegression', 'bench',
                            str(rate), str(block), mode], text=True).strip().split('\t')
                        if len(result) != len(fields): raise ValueError('Incomplete benchmark result')
                        writer.writerow({'round': turn+1, 'processor': label, **dict(zip(fields, result))})
                        target.flush()
    print(f'Wrote 54 independent processor measurements to {args.output}')


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);subs=parser.add_subparsers(dest='action',required=True)
    record=subs.add_parser('capture'); record.add_argument('build',type=Path);record.add_argument('out',type=Path);record.add_argument('legacy',type=Path);record.add_argument('--label',required=True);record.add_argument('--mode',choices=('off','flat'),default='off');record.add_argument('--make-legacy',action='store_true')
    diff=subs.add_parser('compare');diff.add_argument('reference',type=Path);diff.add_argument('candidate',type=Path);diff.add_argument('output',type=Path)
    paired=subs.add_parser('paired');paired.add_argument('reference',type=Path);paired.add_argument('candidate',type=Path);paired.add_argument('output',type=Path)
    args=parser.parse_args();sys.exit({'capture':capture,'compare':compare,'paired':paired_benchmark}[args.action](args))
