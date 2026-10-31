"""Historical1% operator rescreen; preserve original verdicts and make no GEMV/model-speed claim."""
import copy
import importlib.util
import json
import math
from pathlib import Path
import sys

H = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('int8_original_pairs',H/'check_int8_layout_20261025.py')
original = importlib.util.module_from_spec(spec); spec.loader.exec_module(original)


def stats(group):
    assert len(group['pairs']) == 8
    assert all(math.isfinite(row[key]) and row[key] > 0 for row in group['pairs']
               for key in ('a0_us','a1_us','b0_us','b1_us'))
    return original.compare(group['pairs'])


def qualifies(group, threshold):
    s = stats(group)
    return bool(group['quality_passed'] and s['stable_cycles'] >= 6 and s['median_pct'] is not None
                and math.isfinite(s['median_pct']) and s['median_pct'] >= threshold)


def rescreen():
    result = dict(scope='historical-operator-rescreen',threshold_pct=1,production_changed=False,
                  adopted=False,model_speed_acceptance=False,target35_achieved=False,studies=[])
    for name in ('benefit-verify','fusion-stability','hybrid-stability'):
        data = json.loads((H/('int8-'+name+'-2026-10-25.json')).read_text(encoding='utf8'))
        assert data['complete'] and not data['adopted']
        for variant in sorted({c.get('variant','candidate') for c in data['cohorts']}):
            tables = []
            for cohort in data['cohorts']:
                if cohort.get('variant','candidate') != variant: continue
                table = {(r['gu'],g['T'],g['mode']):g for r in cohort['runs'] for g in r['groups']}
                assert len(table) == 48
                for g in table.values():
                    s = stats(g)
                    assert all(g[k] == s[k] for k in ('stable_cycles','median_pct'))
                    assert {e['profile'] for e in g['errors']} == {0,1} and len(g['errors']) == 2
                    quality = all(math.isfinite(e['nrmse']) and math.isfinite(e['cosine'])
                        and e['nrmse'] <= (.1 if e['profile'] else .05)
                        and e['cosine'] >= (.98 if e['profile'] else .995) for e in g['errors'])
                    assert g['quality_passed'] == quality and g['passed'] == qualifies(g,3)
                tables.append(table)
            assert len(tables) == 2 and set(tables[0]) == set(tables[1])
            selected = {threshold:sorted(k for k in tables[0] if all(qualifies(t[k],threshold) for t in tables))
                        for threshold in (1,3)}
            # ponytail: these are candidate seeds from old data; a new protocol must test native grouped GEMV and the model.
            result['studies'].append(dict(study=name,variant=variant,total=48,
                seeds_1pct=selected[1],seeds_3pct=selected[3],
                newly_eligible=sorted(set(selected[1])-set(selected[3])),
                original21_seeds_1pct=sorted(k for k in selected[1] if list(k) in data['original_confirmed']),
                small_batch=[dict(gu=k[0],T=k[1],layout=k[2],gains_pct=[t[k]['median_pct'] for t in tables])
                             for k in selected[1] if k[1] in (1,6)]))
    return result


def check():
    good = dict(quality_passed=True,pairs=[dict(a0_us=101,a1_us=101,b0_us=100,b1_us=100) for _ in range(8)])
    assert qualifies(good,1) and not qualifies(good,3)
    for field,value in (('a0_us',0),('a1_us',float('nan')),('b0_us',float('inf'))):
        bad = copy.deepcopy(good); bad['pairs'][0][field] = value
        try: qualifies(bad,1)
        except AssertionError: pass
        else: raise AssertionError('invalid timing accepted')
    bad = copy.deepcopy(good); bad['quality_passed'] = False; assert not qualifies(bad,1)
    bad = copy.deepcopy(good)
    for row in bad['pairs']: row['a1_us'] = 105
    assert not qualifies(bad,1)
    result = rescreen()
    counts = [(s['study'],s['variant'],len(s['seeds_1pct']),len(s['original21_seeds_1pct'])) for s in result['studies']]
    assert counts == [('benefit-verify','candidate',24,20),('fusion-stability','fused',18,13),
                      ('fusion-stability','split',23,17),('hybrid-stability','fused',10,7),
                      ('hybrid-stability','split',20,17)]
    assert not next(s for s in result['studies'] if s['study']=='hybrid-stability' and s['variant']=='fused')['small_batch']
    print('PASS historical1% rescreen,finite/drift/quality gates and unchanged original verdicts; no native-GEMV/model acceptance')
    return result


if __name__ == '__main__':
    assert sys.argv[1:] in ([],['--self-check'])
    result = check()
    if '--self-check' not in sys.argv: print(json.dumps(result,indent=2))
