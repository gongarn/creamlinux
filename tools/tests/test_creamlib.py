#!/usr/bin/env python3
"""Unit tests for tools/creamlib.py - stdlib only, run via:
    python3 -m unittest discover -s tools/tests
or from CMake CI step.
"""

import os
import shutil
import sys
import tempfile
import unittest
import zipfile
from unittest.mock import patch

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

import creamlib  # noqa: E402


def make_fake_steam(root):
    """Build a fake Steam layout:
    root/steamapps/libraryfolders.vdf + appmanifest_*.acf
    root/steamapps/common/<game>/...
    Returns (root, vdf_path)."""
    steamapps = os.path.join(root, "steamapps")
    common = os.path.join(steamapps, "common")
    os.makedirs(common)
    with open(os.path.join(steamapps, "libraryfolders.vdf"), "w") as fh:
        fh.write('"libraryfolders"\n{\n\t"0"\n\t{\n\t\t"path"\t\t"%s"\n'
                 '\t\t"apps"\n\t\t{\n\t\t\t"111111"\t\t"1"\n'
                 '\t\t\t"222222"\t\t"1"\n\t\t}\n\t}\n}\n' % root)
    with open(os.path.join(steamapps, "appmanifest_111111.acf"), "w") as fh:
        fh.write('"AppState"\n{\n\t"appid"\t\t"111111"\n'
                 '\t"name"\t\t"Fake Native Game"\n'
                 '\t"installdir"\t\t"FakeNative"\n}\n')
    with open(os.path.join(steamapps, "appmanifest_222222.acf"), "w") as fh:
        fh.write('"AppState"\n{\n\t"appid"\t\t"222222"\n'
                 '\t"name"\t\t"Fake Proton Game"\n'
                 '\t"installdir"\t\t"FakeProton"\n}\n')
    # native game: libsteam_api.so in lib/ subfolder, dlc content present
    native = os.path.join(common, "FakeNative")
    os.makedirs(os.path.join(native, "lib"))
    open(os.path.join(native, "lib", "libsteam_api.so"), "w").close()
    dlc = os.path.join(native, "dlc")
    os.makedirs(dlc)
    for i in range(12):
        open(os.path.join(dlc, f"dlc{i:03d}"), "w").close()
    # proton game: steam_api64.dll in bin/x64
    proton = os.path.join(common, "FakeProton")
    os.makedirs(os.path.join(proton, "bin", "x64"))
    open(os.path.join(proton, "bin", "x64", "steam_api64.dll"), "w").close()
    return root, os.path.join(steamapps, "libraryfolders.vdf")


class TestVdf(unittest.TestCase):
    def test_parse_nested(self):
        text = '"a" "1" "b" { "c" "2" }'
        data = creamlib.vdf_to_dict(creamlib.parse_vdf(text))
        self.assertEqual(data, {"a": "1", "b": {"c": "2"}})

    def test_parse_flat(self):
        text = '"x"\n{\n"0" "path"\n"1" "other"\n}\n'
        data = creamlib.vdf_to_dict(creamlib.parse_vdf(text))
        self.assertEqual(data, {"x": {"0": "path", "1": "other"}})

    def test_repeated_keys_become_list(self):
        data = creamlib.vdf_to_dict(creamlib.parse_vdf('"k" "a" "k" "b"'))
        self.assertEqual(data, {"k": ["a", "b"]})


class TestScan(unittest.TestCase):
    def test_scan_games(self):
        root, vdf = make_fake_steam(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, root)
        with patch.object(creamlib, "find_steam_library_roots",
                          return_value=[vdf]):
            games = creamlib.scan_games()
        by_id = {g["appid"]: g for g in games}
        self.assertIn("111111", by_id)
        self.assertIn("222222", by_id)
        self.assertEqual(by_id["111111"]["game_type"], "native")
        self.assertEqual(by_id["222222"]["game_type"], "proton")
        self.assertEqual(by_id["111111"]["dlc_status"], "ok")
        self.assertEqual(by_id["222222"]["dlc_status"], "unknown")
        # tool apps are excluded
        self.assertNotIn("1070560", by_id)

    def test_find_steam_api_files_depth(self):
        root = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, root)
        deep = os.path.join(root, "a", "b", "c")
        os.makedirs(deep)
        open(os.path.join(deep, "libsteam_api.so"), "w").close()
        self.assertEqual(creamlib.find_steam_api_files(root, max_depth=3),
                         (None, None))
        self.assertEqual(creamlib.find_steam_api_files(root, max_depth=4),
                         ("native", os.path.join(deep, "libsteam_api.so")))


class TestInstall(unittest.TestCase):
    def _make_dist(self):
        dist = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, dist)
        for f in creamlib.CREAM_FILES:
            open(os.path.join(dist, f), "w").close()
        return dist

    def test_install_native_dry_run_does_not_copy(self):
        game = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game)
        dist = self._make_dist()
        logs = []
        rc = creamlib.install_native(game, dist, dry_run=True, log=logs.append)
        self.assertEqual(rc, 0)
        self.assertTrue(any("Native mode plan" in l for l in logs))
        self.assertEqual(os.listdir(game), [])

    def test_install_native_copies(self):
        game = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game)
        dist = self._make_dist()
        creamlib.install_native(game, dist, dry_run=False, log=lambda m: None)
        for f in creamlib.CREAM_FILES:
            self.assertTrue(os.path.exists(os.path.join(game, f)), f)

    def _fake_smokeapi_cache(self, member="smoke_api64.dll"):
        cache = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, cache)
        zip_path = os.path.join(cache, "smokeapi-latest.zip")
        with zipfile.ZipFile(zip_path, "w") as zf:
            zf.writestr(member, b"fake-dll")
        # koaloader cache: dry-run only checks file existence
        kzip = os.path.join(cache, "koaloader-latest.zip")
        with zipfile.ZipFile(kzip, "w") as zf:
            zf.writestr("version-64/version.dll", b"fake")
        return cache

    def test_install_proton_hook(self):
        root, vdf = make_fake_steam(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, root)
        with patch.object(creamlib, "find_steam_library_roots",
                          return_value=[vdf]):
            game = creamlib.find_game("222222")
        cache = self._fake_smokeapi_cache()
        rc = creamlib.install_proton(game["game_dir"], cache_dir=cache,
                                     dry_run=False, mode="hook",
                                     log=lambda m: None)
        self.assertEqual(rc, 0)
        target = os.path.join(game["game_dir"], "bin", "x64", "version.dll")
        self.assertTrue(os.path.exists(target))
        self.assertTrue(os.path.exists(
            os.path.join(game["game_dir"], "cream_api.ini")))

    def test_install_proton_dry_run_modes(self):
        root, vdf = make_fake_steam(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, root)
        with patch.object(creamlib, "find_steam_library_roots",
                          return_value=[vdf]):
            game = creamlib.find_game("222222")
        cache = self._fake_smokeapi_cache()
        for mode in ("hook", "koaloader", "proxy"):
            logs = []
            rc = creamlib.install_proton(game["game_dir"], cache_dir=cache,
                                         dry_run=True, mode=mode,
                                         log=logs.append)
            self.assertEqual(rc, 0, mode)
            self.assertTrue(any("plan" in l for l in logs), mode)


class TestUninstall(unittest.TestCase):
    def test_uninstall_native(self):
        game = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game)
        for f in ("cream.sh", "lib64Creamlinux.so", "cream_api.ini"):
            open(os.path.join(game, f), "w").close()
        creamlib.uninstall_native(game, remove_ini=True, log=lambda m: None)
        self.assertEqual(os.listdir(game), [])

    def test_uninstall_proton_restores_o(self):
        root, vdf = make_fake_steam(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, root)
        with patch.object(creamlib, "find_steam_library_roots",
                          return_value=[vdf]):
            game = creamlib.find_game("222222")
        dll_dir = os.path.join(game["game_dir"], "bin", "x64")
        for f in ("version.dll", "smoke_api64.dll", "steam_api64_o.dll"):
            open(os.path.join(dll_dir, f), "w").close()
        creamlib.uninstall_proton(game["game_dir"], log=lambda m: None)
        remaining = os.listdir(dll_dir)
        self.assertNotIn("version.dll", remaining)
        self.assertNotIn("smoke_api64.dll", remaining)
        self.assertNotIn("steam_api64_o.dll", remaining)
        self.assertIn("steam_api64.dll", remaining)  # restored


class TestIni(unittest.TestCase):
    def test_roundtrip(self):
        game = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game)
        creamlib.write_game_ini(game, {"unlockall": "true"},
                                {"disable_steamapps_issubscribedapp": "false"},
                                [["394360", "HOI4"], ["281990", "Stellaris"]],
                                log=lambda m: None)
        ini = creamlib.read_game_ini(game)
        self.assertEqual(ini["config"]["unlockall"], "true")
        self.assertEqual(ini["methods"]
                         ["disable_steamapps_issubscribedapp"], "false")
        self.assertEqual(len(ini["dlc"]), 2)
        self.assertEqual(ini["dlc"][0], ["394360", "HOI4"])
        self.assertIsNone(creamlib.read_game_ini(
            tempfile.mkdtemp()))


if __name__ == "__main__":
    unittest.main()
