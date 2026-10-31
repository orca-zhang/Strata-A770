"""Finish the protection check after the completed retry's summary-print error."""
import json
from pathlib import Path
import runpy

H = Path(__file__).resolve().parent
prior = runpy.run_path(str(H/'run.py'))
prior['verify']()
checks = json.loads((H/'checks/results.json').read_text())
audit = runpy.run_path(str(H/'check_results.py'))
result = audit['audit_timing'](H/'confirm-retry', checks)
assert result['complete'] and result['confirm_passed']
prereg = json.loads((H/'retry-preregistered.json').read_text())
assert audit['sha'](H/'confirm_retry.py') == prereg['script_sha256']
assert audit['sha'](H/'measure/results.json') == prereg['prior_results_sha256']
prior['b'].save(H/'retry-delivery.json', {
    'complete': True, 'production_unchanged': True, 'source_unchanged': True,
    'postcheck_after_summary_print_error': True,
    'error_record': 'retry-summary-error.txt', 'resampled': False,
})
print('PASS saved confirmation data and final protection hashes; no resampling')
