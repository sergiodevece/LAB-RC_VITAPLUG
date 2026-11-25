"""Verify immutable input data and mono-compatible 2x2 matrix mapping."""
from pathlib import Path
import hashlib, json
import numpy as np
from scipy.io import wavfile
root = Path(__file__).resolve().parent
manifest = json.loads((root/'manifest.json').read_text())
for entry in manifest['files']:
    p = root/entry['path']
    assert p.stat().st_size == entry['bytes'], p
    assert hashlib.sha256(p.read_bytes()).hexdigest() == entry['sha256'], p
    if p.suffix == '.wav':
        sr, a = wavfile.read(p)
        assert sr == entry['sample_rate'] == 96000
        assert a.shape == (entry['frames'], entry['channels'])
        assert a.dtype == np.float32 and np.isfinite(a).all()
results = []
for p in sorted((root/'assets/ir/v05').glob('*.wav')):
    _, h = wavfile.read(p)
    _, m = wavfile.read(root/'assets/ir/v06'/(p.stem+'_matrix.wav'))
    err = float(np.max(np.abs(m[:,:2].astype(np.float64)+m[:,2:].astype(np.float64)-h)))
    assert err < 4e-9, (p,err)
    results.append(err)
    print(f'PASS {p.stem}: {err:.9g}')
assert len(results) == 9
print(f'PASS {len(manifest["files"])} hashes; 9 matrix regressions; max error={max(results):.9g}')
