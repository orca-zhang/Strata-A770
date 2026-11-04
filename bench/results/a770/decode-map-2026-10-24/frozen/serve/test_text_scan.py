"""Optional SIMD scanner and Python fallback must agree, including Unicode and SIMD tails."""
import random
import re
import unittest
from unittest.mock import patch

from serve import frontend


class TextScan(unittest.TestCase):
    def test_marker_restore_matches_literal_alternation(self):
        rnd = random.Random(20261024)
        maps = [frontend.LITERAL_TAGS, frontend.literal_tags(["<|im_start|>", "<|im_end|>"]),
                {"<long>": "aa", "<short>": "a", "<dot>": "."},
                {"<short>": "a", "<long>": "aa", "<dot>": "."},
                {"<empty>": "", "<letter>": "a"}]
        for tags in maps:
            marks = {v: k for k, v in tags.items()}
            pattern = re.compile("|".join(map(re.escape, marks)))
            for _ in range(200):
                prompt = "".join(rnd.choice(["a", "b", ".", "\0", "\u4e2d", "\U0001f600", "\ud800", *marks])
                                 for _ in range(rnd.randrange(50)))
                out, spans, pos, n = [], [], 0, 0
                for match in pattern.finditer(prompt):
                    out.append(prompt[pos:match.start()])
                    n += match.start() - pos
                    tag = marks[match.group()]
                    out.append(tag)
                    spans.append((n, n + len(tag)))
                    n += len(tag)
                    pos = match.end()
                out.append(prompt[pos:])
                self.assertEqual(frontend.unmark_think_literals(prompt, tags), ("".join(out), spans))
        self.assertEqual(frontend.unmark_think_literals("\u4e2d plain", {}), ("\u4e2d plain", []))

    def test_python_single_run_fast_path(self):
        parser = frontend.OutputParser()
        with patch.object(frontend, "_text_scan", None):
            for initial in (0, 1, 2):
                parser.ticks = initial
                for text, expected in (("plain", initial), ("`x`", initial),
                                       ("`x", 1 - initial if initial < 2 else 2),
                                       ("``x`y``", (0, 2, 1)[initial])):
                    self.assertEqual(parser._ticks_after(text), expected)
                    self.assertEqual(parser.ticks, initial)

    @unittest.skipIf(frontend._text_scan is None, "optional scanner not built")
    def test_native_marker_restore_at_boundaries(self):
        native = frontend._text_scan
        with patch.object(frontend, "_text_scan", None):
            for marks in ({}, {"a": ""}, {"a": "<ascii>", "z": "\U0001f600"},
                          {"\u4e2d": "<cjk>"}, {"\U000f0e01": "<think>", "\U000f0e06": "</think>"}):
                tags = {tag: mark for mark, tag in marks.items()}
                for width in (*range(34), 63, 64, 65, 255):
                    for char in ("x", "\u00e9", "\u4e2d", "\U0001f600", "\ud800", "\0"):
                        prompt = (char * width + "a\U000f0e01\U000f0e07\U000f0e06z\u4e2d") * 2
                        self.assertEqual(native.restore_markers(prompt, marks),
                                         frontend.unmark_think_literals(prompt, tags))

    @unittest.skipIf(frontend._text_scan is None, "build optional scanner with tools/build_text_scan.py")
    def test_native_matches_python_at_boundaries(self):
        native = frontend._text_scan
        parser = frontend.OutputParser()
        rnd = random.Random(20261024)
        widths = (0, 1, 2, 3, 7, 8, 15, 16, 17, 31, 32, 33, 63, 64, 65, 255, 4097)
        spaces = ("", " \r\n\t", "\u00a0\u2003\x1c", "\u0085\u2028\u2029\u3000")
        with patch.object(frontend, "_text_scan", None):
            for char in ("x", "\u4e2d", "\U0001f600"):
                for offset in range(17):
                    for width in widths:
                        text = char * offset + "`" * width
                        for initial in (0, 1, width):
                            parser.ticks = initial
                            self.assertEqual(native.backticks(text, initial), parser._ticks_after(text))
            for char in ("x", "\u00e9", "\u4e2d", "\U00010060", "\U0001003c", "\U0001f600", "\ud800", "\0"):
                for width in widths:
                    for offset in range(17):
                        text = char * offset + "`" * width + char * 33 + "``" + char + "`" * width
                        for initial in (0, 1, 2, width):
                            parser.ticks = initial
                            self.assertEqual(native.backticks(text, initial), parser._ticks_after(text))
                            self.assertEqual(parser.ticks, initial)
                        for space in spaces:
                            text = char * offset + "</parameter> literal " + "</parameter>" + space
                            for follower in ("<parameter=next>", "</function>", "<parameterX", "\u200b</function>"):
                                for cut in (0, 1, 10, 11, len(follower)):
                                    value = text + follower[:cut]
                                    for final in (False, True):
                                        self.assertEqual(native.parameter_end(value, final), frontend.param_end(value, final))
            parts = ("x", "`", "``", "</parameter>", "<parameter=", "</function>", "<", "\u2003", "\U00010060")
            for _ in range(3000):
                text = "".join(rnd.choice(parts) for _ in range(rnd.randrange(40)))
                parser.ticks = rnd.randrange(8)
                self.assertEqual(native.backticks(text, parser.ticks), parser._ticks_after(text))
                for final in (False, True):
                    self.assertEqual(native.parameter_end(text, final), frontend.param_end(text, final))

    @unittest.skipIf(frontend._text_scan is None, "optional scanner not built")
    def test_native_rejects_invalid_arguments(self):
        native = frontend._text_scan
        with self.assertRaises(ValueError):
            native.backticks("x", -1)
        with self.assertRaises(TypeError):
            native.backticks(b"x", 0)
        for args in ((), ("x",), ("x", 0, 0), ("x", None), ("x", "0"), ("x", 1.5)):
            with self.assertRaises(TypeError):
                native.backticks(*args)
        with self.assertRaises(OverflowError):
            native.backticks("x", 1 << 100)

        class Index:
            def __index__(self):
                return 2

        self.assertEqual(native.backticks("``", Index()), 0)
        with self.assertRaises(TypeError):
            native.parameter_end(b"x", False)
        for start in (-1, 2):
            with self.assertRaises(ValueError):
                native.parameter_end("x", False, start)
        with self.assertRaises(TypeError):
            native.parameter_end("x", False, 0.5)
        with self.assertRaises(OverflowError):
            native.parameter_end("x", False, 1 << 100)
        with self.assertRaises(TypeError):
            native.restore_markers(b"x", {})
        with self.assertRaises(TypeError):
            native.restore_markers("x", [])
        with self.assertRaises(ValueError):
            native.restore_markers("x", {"xx": "tag"})
        with self.assertRaises(TypeError):
            native.restore_markers("x", {"x": 1})

    def test_parameter_end_start_matches_suffix_without_scanning_prefix(self):
        native = frontend._text_scan
        for backend in (None, native) if native is not None else (None,):
            with patch.object(frontend, "_text_scan", backend):
                for char in ("x", "\u00e9", "\u4e2d", "\U0001f600", "\ud800", "\0"):
                    for width in (0, 1, 15, 16, 17, 31, 64):
                        prefix = "</parameter><parameter=ignored>" + char * width
                        for tail in ("</function>", "<parameter=next>", "<param", "", " prose"):
                            text = prefix + "value </parameter> literal </parameter>\u2003" + tail
                            for final in (False, True):
                                for start in (0, len(prefix), len(prefix) + 1, len(text) - 1, len(text)):
                                    expected = frontend.param_end(text[start:], final)
                                    if expected >= 0:
                                        expected += start
                                    self.assertEqual(frontend.param_end(text, final, start), expected,
                                                     (backend, repr(text), start, final))
                for start in (-1, 2):
                    with self.assertRaises(ValueError):
                        frontend.param_end("x", start=start)


if __name__ == "__main__":
    unittest.main()
