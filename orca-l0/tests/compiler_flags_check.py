"""Check native options for ordinary and ESIMD compiler image properties."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "compiler"))
from build import image_flags

for esimd in (False, True):
    prefix = "isEsimdImage=1|1\n" if esimd else ""
    base = "-vc-codegen " if esimd else ""
    for size, option in ((0, "-ze-intel-enable-auto-large-GRF-mode"), (128, ""),
                         (256, "-doubleGRF" if esimd else "-ze-opt-large-register-file"),
                         (512, "-ze-opt-register-file-size=512")):
        actual = image_flags(prefix + f"sycl-grf-size=1|{size}\n", "-cl-fp32-correctly-rounded-divide-sqrt")
        expected = " ".join(filter(None, (base.strip(), option, "-cl-fp32-correctly-rounded-divide-sqrt")))
        assert actual == expected, (actual, expected)
    assert image_flags(prefix) == base.strip()
for bad in ("sycl-grf-size=1|64", "sycl-grf-size=2|256", "sycl-grf-size=1|256\nsycl-grf-size=1|256"):
    try:
        image_flags(bad)
    except ValueError:
        pass
    else:
        raise AssertionError(f"accepted invalid property: {bad}")
assert image_flags("", "-cl-fp32-correctly-rounded-divide-sqrt", True) == "-cl-fp32-correctly-rounded-divide-sqrt -ze-opt-large-register-file"
for conflicting in ("isEsimdImage=1|1", "sycl-grf-size=1|128", "sycl-grf-size=1|256"):
    try:
        image_flags(conflicting, large_grf=True)
    except ValueError:
        pass
    else:
        raise AssertionError("accepted conflicting large GRF override")
print("PASS ordinary/ESIMD GRF options, rounding and malformed properties")
