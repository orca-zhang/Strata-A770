"""Archive raw evidence without shipping dependency libraries or replacing production binaries."""
import json
from pathlib import Path
import shutil
import subprocess
import sys

H=Path(__file__).resolve().parent
R=H.parent
sys.path.insert(0,str(H))
import run as e
import check_results as audit

def main():
    target=R/'bench/results/a770/q8-subgroup-2026-10-24'
    target.mkdir(exist_ok=True)
    assert not any(target.iterdir()), 'Refuse to replace an existing archive'
    prep=json.loads((H/'preparation.json').read_text())
    assert all(e.sha(R/name)==digest for name,digest in prep['protected'].items())
    assert (R/'sycl/src/kernels/cuda/kv_q8.dp.cpp').read_bytes()==(H/'integrated-kv_q8.dp.cpp').read_bytes()
    assert (R/'sycl/src/kernels/kv_steps_parity.cpp').read_text()==(H/'final-kv_steps_parity.cpp').read_text()
    pairs=[]
    for name,execution in [('raw.json','prototype-direct'),('graph-raw.json','prototype-graph'),('integrated-raw.json','retained-direct')]:
        pairs.extend(audit.derive(json.loads((H/name).read_text()),execution))
    report={'complete':True,'pairs':pairs,'default_enabled':False,'source_retained':True,
            'retention_rule':'User accepts positive operator observations; no 1% or 3% operator minimum. Absolute adjacent-baseline time drift above 3% is excluded.',
            'retention_reason':'Positive eligible observations are retained with all negative and excluded rows. Mixed results and broad timing spread do not support enabling the path by default.',
            'stable_operator_speedup_claimed':False,'whole_model_speedup_claimed':False,'whole_model_tests_performed':False,
            'production_unchanged':True,'protected_sha256':prep['protected'],
            'source_sha256':{name:e.sha(R/name) for name in ['sycl/src/kernels/cuda/kv_q8.dp.cpp','sycl/src/kernels/kv_steps_parity.cpp']},
            'cmake_option':'STRATA_SYCL_Q8_SINGLE_SUBGROUP','whole_model_threshold_pct':1,
            'timing':{'direct':{'warmups':512,'batches':9,'iterations':2048},'graph':{'warmups':64,'batches':9,'iterations':128},
                      'processes_per_execution':20,'excluded_graph_comparisons':sum(not p['eligible'] for p in pairs if p['execution']=='prototype-graph')}}
    e.save(H/'results.json',report)
    for p in H.iterdir():
        if p.is_file() and p.suffix in ('.py','.json','.log','.cpp','.txt'):
            shutil.copyfile(p,target/p.name)
    for side in ('baseline','candidate'):
        shutil.copyfile(H/side/'kv_q8.dp.cpp',target/(side+'-kv_q8.dp.cpp'))
    lines=['# A770 Q8 单 subgroup 写入实验（2026-10-24）','',
           '保留默认关闭的实现和全部原始数据；本轮不声明稳定算子或整模型提速。', '',
           '编译选项 `STRATA_SYCL_Q8_SINGLE_SUBGROUP=ON` 让 32 个线程各处理两个值，省掉原来两个 subgroup 之间的共享内存归约和屏障。',
           'OFF 保留 64 线程形状。K/V 编码、FP16 scale、页面定位、主机镜像与 gather 接口相同；CUDA/HIP 未改。', '',
           '平台：Intel Arc A770 16GB，Xeon E5-2696 v4，Windows，驱动 32.0.101.9034，oneAPI 2025.3.2。',
           '仅测算子，没有加载模型或专家缓存。两侧均为同一工具链、冻结头文件、相同依赖库对象；仅替换 Q8 对象。',
           '实际覆盖 head_dim=256、两个 KV 头；没有测试其他头维、GPU 或工具链版本。',
           '关闭分析器，使用共享锁串行运行。每种执行方式两轮独立 A/B/A/B/A，每臂覆盖 1/4/6 tokens。', '',
           '正确性：初版和保留源码的 OFF/ON 均与冻结基线的完整 KV 指纹 `a6aaf1a5dd761f01` 相同。',
           'Q8 编码/scale 与 CPU 参考逐字节一致，gather 最大误差 0.587 个量化步长，位于既有容差内。',
           '补上批量 Q8 本身的位置变化图重放后，每臂检查从 171 项增至 198 项；覆盖缺页、镜像、跨页、环形映射、canary 和 RoPE 三种缩放参考。', '',
           '以下为保留源码的独立 OFF/ON 计时，单位为吞吐变化。† 表示两侧基线耗时绝对漂移超过 3%，保留原始值并排除。',
           '每格两个数分别是两个被相邻基线夹住的候选进程；不能将不同形状的正值拼成整体收益。','',
           '| 写入方式 | tokens | 初筛 B1 / B3 | 复测 B1 / B3 |','|---|---:|---:|---:|']
    for mode in (0,1):
        for n in (1,4,6):
            cells=[]
            for phase in ('screen','confirm'):
                rows=[p for p in pairs if p['execution']=='retained-direct' and p['mode']==mode and p['T']==n and p['phase']==phase]
                cells.append(' / '.join(f"{p['throughput_change_pct']:+.2f}%"+('†' if not p['eligible'] else '') for p in rows))
            lines.append(f"| {'逐 token' if mode==0 else '批量'} | {n} | {' | '.join(cells)} |")
    direct=[p for p in pairs if p['execution']=='retained-direct']
    valid=[p for p in direct if p['eligible']]
    lines += ['',f'保留源码共 24 组对照，{len(valid)} 组合格（{sum(p["throughput_change_pct"]>0 for p in valid)} 组正值，{sum(p["throughput_change_pct"]<0 for p in valid)} 组负值），其余因漂移排除。',
              '按用户“算子正收益可保留”的标准留下可选源码；数值跨度大，正负并存，没有把这些观测称为稳定加速。',
              '初版逐 token 的 6-token 测点有 +1.32% / +2.14% / +1.38% 合格正值，也有 −1.20% 合格负值；初版批量复测包含 −2.48% / −0.80%。', '',
              '图重放计时：24 组中 23 组漂移超限，仅一组合格且为 −10.59%。每批时长还存在上升趋势，不能从中归因出内核收益。',
              '原始 2048 次/批的首个图进程耗时过高而中止；没有完整图对照被丢弃。中止记录在 `graph-aborted.json`，随后两侧统一改为 128 次/批。',
              '该现象与 [此前连续图测量的漂移限制](../../2026-10-14-a770-hc-graph-query-wait/README.md) 一致；本轮未定位驱动原因。',
              '原配置使用 eager 路径，图微基准也不能代表它的整模型效果；没有再次跑未变更的四场景模型矩阵。', '',
              '本轮仅验证独立算子。greedy、固定种子采样、取消、EOS、恢复以及整模型性能没有为此新选项重新验收，故选项保持 OFF，生产 EXE/配置未替换。', '',
              '## 复查与复测','',
              '离线审计只使用 Python 标准库，独立复算全部 60 个计时进程、72 组对照及漂移排除，并验证原始日志和归档哈希：','',
              '```powershell','python bench/results/a770/q8-subgroup-2026-10-24/check_results.py','```','',
              '独立二进制和冻结依赖保留在 `build-sycl-a770-q8-subgroup-20261024/`。在仓库根目录运行下面的入口（每次选择新的 `--out`）：','',
              '```powershell','python -X utf8 build-sycl-deepinfer-20261023/check_bf16_arena.py build-sycl-a770-q8-subgroup-20261024/recheck.py --out build-sycl-a770-q8-subgroup-20261024/check-repeat',
              '# 同一命令加 --timing 可重做两轮直接提交 A/B/A/B/A；不重做已失效的连续图计时。','```','',
              '重新构建时，在已有独立 SYCL 配置命令追加 `-DSTRATA_SYCL_Q8_SINGLE_SUBGROUP=ON`，然后构建 `kv_steps_parity` / `kv_q8_parity`；关闭使用 `=OFF`。',
              '本页百分比来自独立对象重链接；精确命令、库对象/二进制哈希见 `integrated-build.json`，冻结输入与生产哈希见 `preparation.json`。',
              '大体积依赖库和 EXE 留在独立构建目录，未加入结果归档。全部正负值、漂移及每批原始耗时见 `results.json` 和三个 `*-raw.json`。','']
    (target/'README.md').write_text('\n'.join(lines),encoding='utf-8')
    e.save(target/'archive.json',{'sha256':{p.relative_to(target).as_posix():e.sha(p) for p in target.rglob('*') if p.is_file() and p.name!='archive.json'}})
    subprocess.run([sys.executable,'-X','utf8',str(target/'check_results.py')],check=True)
    print('PASS archive:',target)

if __name__=='__main__':
    main()
