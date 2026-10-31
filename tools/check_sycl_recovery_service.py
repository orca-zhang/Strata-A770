"""Exercise the exact recovery profile through the server and mixed 2K/8K history."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import threading
import urllib.request

R = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(R/'tools'))
import bench_deepinfer as b
from check_orca_http import RecordingEngine, api_result
from check_orca_regression import runtime_env
from serve.frontend import ChatTemplate
from serve.server import Server, Service, engine_args, child_env, make_handler
from strata_tokenizer import Tokenizer

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('--config', type=Path, required=True)
ap.add_argument('--out', type=Path, required=True)
args = ap.parse_args()
out = args.out.resolve()
out.mkdir(exist_ok=False)
cfg = json.loads(args.config.read_text(encoding='utf-8'))
sys.path.insert(0, str(R/'sycl/serve'))
import server_intel as intel
intel.install_xe_reader()
intel.install_switcher(['--config', str(args.config)])
intel.install_logprobs()
spec = importlib.util.spec_from_file_location('memory_metrics', R/'build-sycl-a770-xpu-mtp-q4-20261028/measure_greedy.py')
metrics = importlib.util.module_from_spec(spec)
spec.loader.exec_module(metrics)
directory = Path(cfg['tokenizer'])
vocab = json.loads((directory/'vocab.json').read_text(encoding='utf-8'))
tokens = [None]*len(vocab)
for token, number in vocab.items():
    tokens[number] = token
tokenizer = Tokenizer(tokens, (directory/'merges.txt').read_text(encoding='utf-8').splitlines(),
                      json.loads((directory/'token_type.json').read_text(encoding='utf-8')))
corpus = json.loads((R/'bench/results/a770/orca-assistant-corpus.json').read_text(encoding='utf-8'))['cases']
files = [args.config, Path(cfg['exe']), Path(__file__), metrics.MONITOR, Path(metrics.__file__),
         R/'serve/server.py', R/'serve/frontend.py', R/'tools/check_orca_http.py', Path(intel.__file__)]
hashes = {str(p.resolve()): b.sha(p) for p in files}
d = dict(complete=False, passed=False, config=cfg, files_sha256=hashes, http=[], history=[])
save = lambda: b.save(out/'results.json',d)
save()
with b.gpu_lock():
    engine = server = monitor = None
    try:
        env, selected = runtime_env(child_env(cfg), cfg.get('lib_dirs',[]))
        d.update(args=engine_args(cfg), runtime_env=selected)
        assert d['args'] == cfg['args']
        with (out/'memory.stdout').open('x',encoding='utf-8') as stdout, (out/'memory.stderr').open('x',encoding='utf-8') as stderr:
            monitor = subprocess.Popen([str(metrics.MONITOR),'3600'],cwd=R,env=env,stdout=stdout,stderr=stderr)
        engine = RecordingEngine(cfg['exe'], d['args'], cwd=cfg['cwd'], env=env, log=str(out/'engine.stderr'))
        engine.silence_s = intel.S.engine_silence_s(cfg)
        engine.records = []
        d['info'] = dict(engine.info)
        service = Service(engine, tokenizer, ChatTemplate(directory/'chat_template.jinja'),
                          model_name=cfg['model_name'],serve_quiet=True)
        server = Server(('127.0.0.1',0), make_handler(service))
        thread = threading.Thread(target=server.serve_forever,daemon=True)
        thread.start()
        base = f'http://127.0.0.1:{server.server_address[1]}'
        for path in ('/health','/v1/models'):
            with urllib.request.urlopen(base+path,timeout=30) as response:
                value = json.load(response)
            assert value.get('loaded') if path=='/health' else value['data']
        messages = [dict(role='user',content='Explain why the sky is blue in three sentences.')]
        common = dict(model=cfg['model_name'],temperature=0,max_tokens=64,messages=messages)
        requests = [('/v1/chat/completions',dict(common,reasoning_effort='none')),
                    ('/v1/chat/completions',dict(common,reasoning_effort='none',stream=True)),
                    ('/v1/messages',dict(common,thinking=dict(type='disabled'))),
                    ('/v1/responses',dict(model=cfg['model_name'],temperature=0,max_output_tokens=64,
                                          input=messages,reasoning=dict(effort='none')))]
        for path,body in requests:
            request = urllib.request.Request(base+path,data=json.dumps(body).encode(),headers={'Content-Type':'application/json'})
            with urllib.request.urlopen(request,timeout=1200) as response:
                raw = response.read().decode('utf-8')
                assert response.status==200
            output = api_result(path,raw,body.get('stream'))
            row = engine.records[-1]
            assert row['tokens'] and any(row['tokens']) and row['done']['generated']==len(row['tokens'])
            d['http'].append(dict(path=path,stream=body.get('stream',False),output=output,**row))
            save()
            print('HTTP PASS',path,len(row['tokens']),flush=True)
        references = {}
        for name in ('code','long','prose','followup','code','long'):
            case = corpus[name]
            ids = case['tokens'][:case['prompt_tokens']]
            generated = list(engine.generate(ids,64,{'temperature':0},threading.Event()))
            generated = [t for t in generated if t is not None]
            d['history'].append(dict(case=name,prompt_tokens=len(ids),tokens=generated,done=dict(engine.last)))
            save()
            assert 0<len(generated)<=64 and any(generated),(name,'invalid output',engine.last)
            assert engine.last['generated']==len(generated)
            assert generated==references.setdefault(name,generated),(name,'history changed output')
            assert monitor.poll() is None
            save()
            print('HISTORY PASS',name,len(generated),flush=True)
        d['complete'] = True
    except BaseException as error:
        d['error'] = repr(error)
        raise
    finally:
        if server:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
        if engine:
            proc = engine.proc
            engine.close()
            d['exit_code'] = proc.returncode
        if monitor:
            monitor.terminate()
            monitor.wait(timeout=10)
            d['memory_samples'] = metrics.memory_samples((out/'memory.stdout').read_text(encoding='utf-8'))
            d['min_free_mib'] = min(s['free_bytes'] for s in d['memory_samples'])/2**20
        d['files_unchanged'] = all(b.sha(Path(p))==h for p,h in hashes.items())
        d['passed'] = d['complete'] and d.get('exit_code')==0 and d['files_unchanged'] and d['min_free_mib']>=1024
        save()
assert d['passed']
print('PASS exact recovery profile HTTP/history',d['min_free_mib'],'MiB',flush=True)
