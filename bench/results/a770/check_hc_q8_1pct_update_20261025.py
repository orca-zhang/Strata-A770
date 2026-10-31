"""Apply the user's 1% screen to the preserved HC-Q8 cohort; no new measurements."""
import hashlib
import importlib.util
import json
from pathlib import Path

H = Path(__file__).resolve().parent
BASE = H / 'hc-q8-model-2026-10-25.json'
UPDATE = H / 'hc-q8-model-1pct-update-2026-10-25.json'


def evaluate(d):
    return all(p['throughput_change_pct'] >= 1 and abs(p['baseline_drift_pct']) <= 3
               for p in d['performance']['comparison'].values())


def check():
    spec = importlib.util.spec_from_file_location('hc_original', H / 'check_hc_q8_model_20261025.py')
    original = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(original)
    d = json.loads(BASE.read_text(encoding='utf-8'))
    assert original.sha(H / 'check_hc_q8_model_20261025.py') == d['checker_sha256']
    assert original.sha(H / 'hc-q8-model-2026-10-25.md') == d['report_sha256']
    original.check(d)
    update = json.loads(UPDATE.read_text(encoding='utf-8'))
    assert update['source_sha256'] == hashlib.sha256(BASE.read_bytes()).hexdigest()
    assert update['minimum_gain_pct'] == 1 and update['maximum_baseline_drift_pct'] == 3
    assert update['previous_minimum_gain_pct'] == 3
    assert update['previous_screen_passed'] == d['performance']['passes_screen']
    assert update['updated_screen_passed'] == evaluate(d)
    assert update['comparison'] == d['performance']['comparison']
    assert update['limited_quality_passed'] == d['quality']['passes_quality_screen']
    assert not update['independent_confirmation_complete']
    assert not update['production_changed'] and not update['target_achieved']
    # ponytail: boundary checks suffice for this rule-only update; the original checker audits all measurements.
    fixture = {'performance': {'comparison': {'Code2K': {'throughput_change_pct': 1, 'baseline_drift_pct': 3},
                                             'Prose2K': {'throughput_change_pct': 1, 'baseline_drift_pct': -3}}}}
    assert evaluate(fixture)
    fixture['performance']['comparison']['Code2K']['throughput_change_pct'] = 0.999
    assert not evaluate(fixture)
    fixture['performance']['comparison']['Code2K']['throughput_change_pct'] = 1
    fixture['performance']['comparison']['Prose2K']['baseline_drift_pct'] = 3.001
    assert not evaluate(fixture)
    print('PASS HC-Q8 updated 1% screen, original measurements and rule boundaries; independent confirmation pending')


if __name__ == '__main__':
    check()
