#!/usr/bin/env python3
"""Regression tests for the tools/setup.py CLI wiring (stdlib only), run via:
    python3 -m unittest discover -s tools/tests

Bug: `--update-dlc` never reached the game's cream_api.ini. A standalone run
merged the fetched list into the repo checkout, `--dir <game> --update-dlc`
ignored `--dir` (the per-game branch was unreachable dead code) and
`--install <appid> --update-dlc <appid>` skipped the refresh entirely, so the
DLC list the user asked for landed nowhere the game could read it.
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

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))

import creamlib  # noqa: E402

_SPEC = importlib.util.spec_from_file_location(
    "cream_setup", os.path.join(HERE, "..", "setup.py"))
setup = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(setup)


def fake_game(game_dir, appid="394360", name="Hearts of Iron IV"):
    return {"appid": appid, "name": name, "game_dir": game_dir,
            "game_type": "native", "installed": None}


class SetupCliTestCase(unittest.TestCase):
    def setUp(self):
        self.events = []

        def fake_update(appid, game_dir=None, dry_run=False, log=print):
            self.events.append(("update", str(appid), game_dir, dry_run))
            return 0

        patcher = patch.object(creamlib, "run_dlc_update",
                               side_effect=fake_update)
        patcher.start()
        self.addCleanup(patcher.stop)

    def run_cli(self, *argv):
        with patch.object(sys, "argv", ["setup.py", *argv]):
            return setup.main()

    def updates(self):
        return [e for e in self.events if e[0] == "update"]


class TestUpdateDlcTarget(SetupCliTestCase):
    def test_standalone_update_targets_installed_game(self):
        game_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game_dir)
        with patch.object(creamlib, "find_game",
                          return_value=fake_game(game_dir)):
            rc = self.run_cli("--update-dlc", "394360")
        self.assertEqual(rc, 0)
        self.assertEqual(self.updates(),
                         [("update", "394360", game_dir, False)])
        # never the repo checkout / process CWD
        self.assertNotEqual(game_dir, creamlib.REPO_ROOT)

    def test_dir_with_update_installs_then_refreshes(self):
        game_dir = tempfile.mkdtemp()
        dist = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game_dir)
        self.addCleanup(shutil.rmtree, dist)
        for name in creamlib.CREAM_FILES:
            open(os.path.join(dist, name), "w").close()

        def fake_install(*args, **kwargs):
            self.events.append(("install",))
            return 0

        with patch.object(creamlib, "detect_mode", return_value="native"), \
                patch.object(creamlib, "find_creamlinux_dist",
                             return_value=dist), \
                patch.object(creamlib, "install_native",
                             side_effect=fake_install):
            rc = self.run_cli("--dir", game_dir, "--update-dlc", "394360")
        self.assertEqual(rc, 0)
        # the install copies the distribution ini over the game's file, so the
        # refresh has to run last or the fetched list is wiped
        self.assertEqual(self.events,
                         [("install",), ("update", "394360", game_dir, False)])

    def test_install_and_update_both_run_update_last(self):
        game_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game_dir)

        def fake_install(*args, **kwargs):
            self.events.append(("install",))
            return 0

        with patch.object(creamlib, "find_game",
                          return_value=fake_game(game_dir)), \
                patch.object(creamlib, "detect_mode", return_value="native"), \
                patch.object(creamlib, "find_creamlinux_dist",
                             return_value="/dist"), \
                patch.object(creamlib, "install_native",
                             side_effect=fake_install):
            rc = self.run_cli("--install", "394360", "--update-dlc", "394360")
        self.assertEqual(rc, 0)
        # the install copies the distribution ini over the game's file, so the
        # refresh has to happen afterwards or it is lost
        self.assertEqual(self.events,
                         [("install",), ("update", "394360", game_dir, False)])

    def test_unknown_app_errors_without_writing_anything(self):
        stderr = io.StringIO()
        with patch.object(creamlib, "find_game", return_value=None), \
                contextlib.redirect_stderr(stderr):
            rc = self.run_cli("--update-dlc", "999999")
        self.assertEqual(rc, 1)
        self.assertEqual(self.updates(), [])
        self.assertIn("not found in Steam libraries", stderr.getvalue())
        self.assertIn("update-dlc.py --output", stderr.getvalue())

    def test_dry_run_is_forwarded(self):
        game_dir = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, game_dir)
        with patch.object(creamlib, "find_game",
                          return_value=fake_game(game_dir)):
            rc = self.run_cli("--update-dlc", "394360", "--dry-run")
        self.assertEqual(rc, 0)
        self.assertEqual(self.updates(),
                         [("update", "394360", game_dir, True)])


if __name__ == "__main__":
    unittest.main()
