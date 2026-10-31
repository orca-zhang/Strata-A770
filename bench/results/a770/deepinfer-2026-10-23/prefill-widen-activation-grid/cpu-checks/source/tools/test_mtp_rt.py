"""Run with python tools/test_mtp_rt.py; no GPU or model files required."""
import numpy as np

from mtp_rt import blob_of, H, FF


def main():
    gu = np.arange(2 * FF * (H // 32 * 34), dtype=np.uint8).reshape(2 * FF, -1)
    gu[FF:] ^= 0x5A
    down = np.full((H, FF // 32 * 34), 173, dtype=np.uint8)
    blob = blob_of(gu, down, q8=True)
    gate_bytes = FF * H // 32 * 34
    assert len(blob) == 3 * gate_bytes
    assert blob[:gate_bytes] == gu[:FF].tobytes()
    assert blob[gate_bytes:2 * gate_bytes] == gu[FF:].tobytes()
    assert blob[2 * gate_bytes:] == down.tobytes()
    try:
        blob_of(gu[:, :-1], down, q8=True)
    except ValueError:
        pass
    else:
        raise AssertionError("malformed Q8 expert rows accepted")
    print("PASS MTP Q8_0 native gate/up/down layout and shape rejection")


if __name__ == "__main__":
    main()
