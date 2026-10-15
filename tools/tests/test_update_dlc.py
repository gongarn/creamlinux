#!/usr/bin/env python3
"""Unit tests for tools/update-dlc.py (IniFile model + merge/dedupe logic;
network access is not exercised). Run via:
    python3 -m unittest discover -s tools/tests
"""

import contextlib
import importlib.util
import io
import os
import shutil
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

_SPEC = importlib.util.spec_from_file_location(
    "update_dlc", os.path.join(os.path.dirname(__file__), "..", "update-dlc.py"))
update_dlc = importlib.util.module_from_spec(_SPEC)
assert _SPEC.loader is not None
_SPEC.loader.exec_module(update_dlc)


class TestIniModel(unittest.TestCase):
    def test_parse_render_roundtrip(self):
        text = ("# comment\n"
                "[config]\n"
                "unlockall = true\n"
                "\n"
                "[dlc]\n"
                "123 = First\n"
                "456 = Second\n")
        ini = update_dlc.IniFile.parse(text)
        self.assertEqual(ini.get("config", "unlockall"), "true")
        self.assertEqual(ini.get("dlc", "123"), "First")
        self.assertIsNone(ini.get("dlc", "999"))
        rendered = ini.render()
        self.assertIn("# comment", rendered)
        self.assertIn("[config]", rendered)
        self.assertIn("123 = First", rendered)

    def test_set_preserves_order(self):
        ini = update_dlc.IniFile.parse("[dlc]\n123 = A\n")
        ini.set("dlc", "456", "B")
        ini.set("dlc", "123", "A2")
        self.assertEqual(ini.get("dlc", "123"), "A2")
        self.assertEqual(ini.get("dlc", "456"), "B")
        self.assertEqual(ini.render().count("123 ="), 1)


class TestMergeDedupe(unittest.TestCase):
    def test_merge_adds_and_skips(self):
        ini = update_dlc.IniFile.parse("[dlc]\n100 = Old\n")
        added, updated, skipped = update_dlc.merge_dlcs(
            ini, [(200, "New"), (100, "Old")], refresh_names=False)
        self.assertEqual((added, updated, skipped), (1, 0, 1))
        self.assertEqual(ini.get("dlc", "200"), "New")

    def test_merge_refresh_names(self):
        ini = update_dlc.IniFile.parse("[dlc]\n100 = Old\n")
        _, updated, _ = update_dlc.merge_dlcs(
            ini, [(100, "Renamed")], refresh_names=True)
        self.assertEqual(updated, 1)
        self.assertEqual(ini.get("dlc", "100"), "Renamed")

    def test_dedupe_keeps_first(self):
        ini = update_dlc.IniFile.parse(
            "[dlc]\n"
            "100 = First\n"
            "200 = B\n"
            "100 = First\n"      # duplicate
            "200 = B\n"          # duplicate
            "# comment stays\n"
            "300 = C\n")
        removed = update_dlc.dedupe_dlc_section(ini)
        self.assertEqual(removed, 2)
        text = ini.render()
        self.assertEqual(text.count("100 = First"), 1)
        self.assertEqual(text.count("200 = B"), 1)
        self.assertEqual(text.count("300 = C"), 1)
        self.assertIn("# comment stays", text)
        # index view must stay consistent after removal
        self.assertEqual(ini.get("dlc", "100"), "First")
        self.assertEqual(ini.get("dlc", "300"), "C")

    def test_dedupe_on_file_with_duplicates(self):
        """End-to-end: parse a file with dupes, dedupe, re-render."""
        with tempfile.NamedTemporaryFile("w", suffix=".ini",
                                         delete=False) as fh:
            fh.write("[dlc]\n1657990 = Mars Lifestyle Radio\n"
                     "1657990 = Mars Lifestyle Radio\n")
            path = fh.name
        self.addCleanup(os.unlink, path)
        with open(path, "r", encoding="utf-8") as fh:
            ini = update_dlc.IniFile.parse(fh.read())
        removed = update_dlc.dedupe_dlc_section(ini)
        self.assertEqual(removed, 1)
        with open(path, "w", encoding="utf-8") as fh:
            fh.write(ini.render())
        with open(path, "r", encoding="utf-8") as fh:
            self.assertEqual(fh.read().count("1657990 ="), 1)


class TestLineEndings(unittest.TestCase):
    """package/cream_api.ini is CRLF - a rewrite must not turn it into LF."""

    def test_crlf_roundtrip(self):
        text = ("[config]\r\nunlockall = false\r\n\r\n"
                "[dlc]\r\n100 = A\r\n")
        self.assertEqual(update_dlc.IniFile.parse(text).render(), text)

    def test_lf_roundtrip(self):
        text = "[dlc]\n100 = A\n"
        self.assertEqual(update_dlc.IniFile.parse(text).render(), text)

    def test_load_save_preserves_crlf(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        path = os.path.join(tmp, "cream_api.ini")
        with open(path, "wb") as fh:
            fh.write(b"[dlc]\r\n100 = Old\r\n")
        ini = update_dlc.load_ini(path)
        update_dlc.merge_dlcs(ini, [(200, "New")], refresh_names=False)
        update_dlc.save_ini(path, ini)
        with open(path, "rb") as fh:
            raw = fh.read()
        self.assertIn(b"200 = New\r\n", raw)
        self.assertEqual(raw.count(b"\n"), raw.count(b"\r\n"))

    def test_load_missing_file_is_empty(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        ini = update_dlc.load_ini(os.path.join(tmp, "nope.ini"))
        self.assertEqual(ini.sections, [])


class TestDryRun(unittest.TestCase):
    def test_main_dry_run_prints_diff_without_touching_file(self):
        tmp = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, tmp)
        path = os.path.join(tmp, "cream_api.ini")
        with open(path, "wb") as fh:
            fh.write(b"[dlc]\r\n100 = Old\r\n")

        def fake_fetch(appid):
            if str(appid) == "1":
                return "Test Game", [100, 200]
            return "Old", []

        buf = io.StringIO()
        with patch.object(update_dlc, "fetch_app", side_effect=fake_fetch), \
                patch.object(update_dlc, "fetch_dlc_names",
                             return_value={100: "Old", 200: "New"}), \
                patch.object(sys, "argv",
                             ["update-dlc.py", "1", "--output", path,
                              "--dry-run"]), \
                contextlib.redirect_stdout(buf):
            update_dlc.main()
        out = buf.getvalue()
        self.assertIn("+ 200 = New", out)
        self.assertIn("Merge result: 1 added", out)
        # the old dry-run dumped the whole merged file and buried the summary
        self.assertNotIn("[dlc]", out)
        with open(path, "rb") as fh:
            self.assertEqual(fh.read(), b"[dlc]\r\n100 = Old\r\n")

    def test_preview_is_bounded(self):
        changes = [("+", str(1000 + i), f"name {i}")
                   for i in range(update_dlc.DRY_RUN_PREVIEW_LIMIT + 10)]
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            update_dlc.print_dry_run(changes)
        lines = buf.getvalue().splitlines()
        self.assertLessEqual(len(lines),
                             update_dlc.DRY_RUN_PREVIEW_LIMIT + 3)
        self.assertIn("... and 10 more", lines[-1])


if __name__ == "__main__":
    unittest.main()
