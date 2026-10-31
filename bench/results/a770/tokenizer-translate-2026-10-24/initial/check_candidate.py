import importlib.util,sys,unittest
from pathlib import Path
r=Path.cwd();f=r/'build-sycl-a770-tokenizer-translate-20261024'
p=f/'candidate/strata_tokenizer.py'
spec=importlib.util.spec_from_file_location('strata_tokenizer',p)
mod=importlib.util.module_from_spec(spec);sys.modules[spec.name]=mod;spec.loader.exec_module(mod)
sys.path.insert(0,str(r))
suite=unittest.defaultTestLoader.loadTestsFromNames(['serve.test_detok','serve.test_server.LiteralThinkTags','serve.test_server.LiteralControlTokens','serve.test_server.ImageMarkers'])
result=unittest.TextTestRunner(verbosity=2).run(suite)
if not result.wasSuccessful():raise SystemExit(1)
print('CHECK_SUMMARY',result.testsRun,len(result.failures),len(result.errors),len(result.skipped))
