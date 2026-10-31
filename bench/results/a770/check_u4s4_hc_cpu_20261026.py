"""CPU precision screen of cheaper HC digit combinations; no GPU or timing claims."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, struct
import numpy as np

H = Path(__file__).resolve().parent
R = H.parents[2]
spec = importlib.util.spec_from_file_location('packer', R/'tools/u4s4_pack.py')
packer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packer)
D, N, LR, T = 10240, 2560, 320, 8

def bf16(raw, shape):
    return (np.frombuffer(raw, '<u2').astype(np.uint32) << 16).view(np.float32).reshape(shape)

def planes(raw, rows, cols):
    payload = packer.quantize(raw, 'bf16', rows, cols, 2)
    words = np.frombuffer(payload, '<u4', 2*rows*cols//8, 32).reshape(2, rows//8, cols//64, 8, 8)
    codes = ((words[..., None] >> (np.arange(8, dtype=np.uint32)*4)) & 15).astype(np.int32)-8
    codes = codes.transpose(0, 1, 4, 2, 3, 5).reshape(2, rows, cols)
    scales = np.frombuffer(payload, '<f4', offset=32+2*rows*cols//8*4)
    scales = scales.reshape(2, rows//8, cols//64, 8).transpose(0, 1, 3, 2).reshape(2, rows, cols//64)
    return codes.astype(np.float32)*np.repeat(scales, 64, axis=-1)

def activation(x):
    residual = x.copy().reshape(T, -1, 64)
    out = []
    for _ in range(2):
        maximum = np.max(np.abs(residual), axis=-1, keepdims=True)
        scale = np.where(maximum > 0, np.maximum(maximum/np.float32(7), np.float32(2**-126)), np.float32(1))
        quant = np.clip(np.rint(residual/scale), -7, 7)
        digit = quant*scale
        out.append(digit.reshape(x.shape))
        residual = (residual.astype(np.float64)-quant.astype(np.float64)*scale).astype(np.float32)
    return out

def metrics(a, b):
    a, b = a.astype(np.float64).ravel(), b.astype(np.float64).ravel()
    aa, bb = np.dot(a, a), np.dot(b, b)
    return dict(nrmse=float(np.sqrt(np.sum((a-b)**2)/(aa+1e-300))),
                cosine=float(np.dot(a, b)/np.sqrt(aa*bb)) if aa>0 and bb>0 else float(aa==bb))

def sigmoid(x):
    return np.reciprocal(1+np.exp(-x.astype(np.float64))).astype(np.float32)

def projection(x, w, mode):
    a = activation(x)
    if mode == 'w4a8':
        return (a[0]+a[1])@w[0].T
    if mode == 'w8a4':
        return a[0]@(w[0]+w[1]).T
    if mode == 'w4a4':
        return a[0]@w[0].T
    if mode == 'three-dpas':
        return a[0]@(w[0]+w[1]).T+a[1]@w[0].T
    assert mode == 'four-dpas'
    return (a[0]+a[1])@(w[0]+w[1]).T

def selfcheck():
    x = (np.arange(8*128, dtype=np.float32).reshape(8,128)%39-19)/8
    raw = (x.view(np.uint32)>>16).astype('<u2').tobytes()
    w = planes(raw, 8, 128)
    assert np.max(np.abs(x-w.sum(axis=0))) < .02
    a = activation(x)
    assert np.max(np.abs(x-sum(a))) < .02
    for mode in ('four-dpas','three-dpas','w4a8','w8a4','w4a4'):
        assert np.isfinite(projection(x, w, mode)).all()
    # Independent integer reference: apply the U4 zero point for each S4 digit.
    qx=(np.arange(2*8*64).reshape(2,8,64)*13%15-7).astype(np.int32)
    qw=(np.arange(2*8*64).reshape(2,8,64)*11%15-7).astype(np.int32)
    three=sum((qx[a]@(qw[b]+8).T-8*qx[a].sum(axis=1,keepdims=True))*2.**(-a*4-b*5)
              for a,b in ((0,0),(0,1),(1,0)))
    full=(qx[0]+qx[1]/16)@(qw[0]+qw[1]/32).T
    assert np.array_equal(three,full-(qx[1]@qw[1].T)/512)
    print('PASS native decoder and activation residual self-check', flush=True)

def measure(bundle, output):
    selfcheck()
    seed = 12345
    def random(count):
        nonlocal seed
        result = np.empty(count, np.float32)
        for i in range(count):
            seed = (seed*1664525+1013904223)&0xffffffff
            result[i] = ((seed>>8)&0xffff)/65536-.5
        return result
    inputs = random(T*D).reshape(T,D)
    bo, ip = random(T*N).reshape(T,1,N)*.25, random(T*4).reshape(T,4,1)
    rows = []
    with bundle.open('rb') as f:
        assert f.read(8)==b'HCP8v001' and struct.unpack('<I',f.read(4))[0]==3
        for ci in range(3):
            inject = struct.unpack('<I',f.read(4))[0]
            downraw, upraw = f.read(LR*D*2), f.read(D*LR*2)
            down, up = bf16(downraw,(LR,D)), bf16(upraw,(D,LR))
            wd, wu = planes(downraw,LR,D), planes(upraw,D,LR)
            f.seek(2*6553600+2*3481600,1)
            norm = np.frombuffer(f.read(D*4),'<f4')
            injectraw = f.read(4*D*2)
            wi = planes(injectraw+bytes(4*D*2),8,D) if inject else None
            injection = bf16(injectraw,(4,D)) if inject else None
            print('CASE', ci, 'packed pristine BF16', flush=True)
            for profile in range(3):
                sample = inputs.copy()
                if profile==1: sample *= .001
                if profile==2: sample.reshape(-1)[::256] *= 16
                for apply in (False, True):
                    rv = sample.reshape(T,4,N).copy()
                    if apply: rv = (rv.astype(np.float64)+bo.astype(np.float64)*2*sigmoid(ip/4)).astype(np.float32)
                    rs = np.reciprocal(np.sqrt(np.mean(rv.astype(np.float64)**2,axis=-1,keepdims=True)+1e-6)).astype(np.float32)
                    xn = (rv*norm.reshape(4,N)*rs).reshape(T,D)
                    z = (xn@down.T)/4
                    lo = z*sigmoid(z)
                    baseline = np.mean(xn.reshape(T,4,N)*sigmoid(lo@up.T).reshape(T,4,N),axis=1)
                    inj = xn@injection.T if inject else None
                    for scope in ('up','both'):
                        for mode in ('four-dpas','three-dpas','w4a8','w8a4','w4a4'):
                            low = lo
                            if scope=='both':
                                zq = projection(xn,wd,mode)/4
                                low = zq*sigmoid(zq)
                            mixed = np.mean(xn.reshape(T,4,N)*sigmoid(projection(low,wu,mode)).reshape(T,4,N),axis=1)
                            iq = (projection(xn,wi,mode)[:,:4] if scope=='both' else inj) if inject else None
                            for count in (1,4,6,8):
                                fields=[('lo',lo,low),('mixed',baseline,mixed)]
                                if inject: fields.append(('inject',inj,iq))
                                for field,reference,candidate in fields:
                                    m = metrics(reference[:count],candidate[:count])
                                    m['passed'] = m['nrmse'] <= (.1 if profile==2 else .05) and m['cosine'] >= (.98 if profile==2 else .995)
                                    rows.append(dict(case=ci,profile=profile,apply=apply,T=count,scope=scope,mode=mode,field=field,**m))
            del wd, wu, down, up
        assert f.read(1)==b''
    result = dict(cpu_only=True,gpu_timing=False,model_quality=False,source='Pristine BF16, native packer decoded on CPU; FP32 matmul and F64 norm reduction, not exact GPU accumulation.',
                  bundle_sha256=hashlib.sha256(bundle.read_bytes()).hexdigest(),
                  script_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                  packer_sha256=hashlib.sha256((R/'tools/u4s4_pack.py').read_bytes()).hexdigest(),
                  platform='CPU NumPy 2.4.6; OPENBLAS_NUM_THREADS=2; OMP_NUM_THREADS=2',rows=rows)
    with output.open('x',encoding='utf-8') as f:
        f.write(json.dumps(result,indent=2))
    for scope in ('up','both'):
        for mode in ('four-dpas','three-dpas','w4a8','w8a4','w4a4'):
            cells=[r for r in rows if r['scope']==scope and r['mode']==mode]
            print(scope,mode,'passed',sum(r['passed'] for r in cells),'/',len(cells),'worst',max(r['nrmse'] for r in cells),flush=True)

def audit(path):
    d=json.loads(path.read_text(encoding='utf-8'))
    assert d['cpu_only'] and not d['gpu_timing'] and not d['model_quality']
    assert d['script_sha256']==hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    assert d['packer_sha256']==hashlib.sha256((R/'tools/u4s4_pack.py').read_bytes()).hexdigest()
    modes=('four-dpas','three-dpas','w4a8','w8a4','w4a4')
    expected={(c,p,a,t,s,m,f) for c in range(3) for p in range(3) for a in (False,True)
              for t in (1,4,6,8) for s in ('up','both') for m in modes
              for f in (('lo','mixed','inject') if c<2 else ('lo','mixed'))}
    rows=d['rows']
    assert len(rows)==1920
    assert {(v['case'],v['profile'],v['apply'],v['T'],v['scope'],v['mode'],v['field']) for v in rows}==expected
    for v in rows:
        assert np.isfinite(v['nrmse']) and np.isfinite(v['cosine'])
        assert v['passed']==(v['nrmse']<=(.1 if v['profile']==2 else .05) and v['cosine']>=(.98 if v['profile']==2 else .995))
    for scope in ('up','both'):
        for mode in modes:
            cells=[v for v in rows if v['scope']==scope and v['mode']==mode]
            passed=sum(v['passed'] for v in cells)
            assert len(cells)==192
            if mode in ('three-dpas','four-dpas'):assert passed==192
            else:assert passed<192
    print('PASS 1,920 CPU precision cells; three-DPAS up/both 384/384; no GPU or model speed claim')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bundle',type=Path,help='Local pristine BF16 HCP8v001 bundle; never a Q2 input')
    p.add_argument('--out',type=Path)
    args=p.parse_args()
    if args.bundle:
        if args.out is None or args.out.exists():p.error('--out must be a new file')
        measure(args.bundle,args.out)
        audit(args.out)
    else:
        if args.out is not None:p.error('--out requires --bundle')
        selfcheck()
        audit(Path(__file__).with_name('u4s4-hc-cpu-2026-10-26.json'))

if __name__=='__main__':main()
