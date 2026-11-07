"""Check fixture identity and reject a corrupt or escaping catalog input."""
from pathlib import Path
import importlib.util
import json
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("fixture_paths", ROOT / "tests/fixtures/paths.py")
paths = importlib.util.module_from_spec(spec)
spec.loader.exec_module(paths)


class FixtureCatalogTest(unittest.TestCase):
    def test_supplied_assets_and_separate_clients(self):
        self.assertGreater(paths.verify(), 48)
        self.assertNotEqual(paths.fixture_path("jcalgtest.client").read_bytes(),
                            paths.fixture_path("api.client").read_bytes())
        self.assertIn("1.8.4", str(paths.fixture_path("jcalgtest.cap")))

    def test_corruption_and_path_escape(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            location = root / "tests/fixtures/manifest.json"
            location.parent.mkdir(parents=True)
            record = {"path": "input.cap", "size": 1, "sha256": "0" * 64}
            location.write_text(json.dumps({"assets": {"cap": "input.cap"}, "files": [record]}))
            (root / "input.cap").write_bytes(b"x")
            with self.assertRaisesRegex(ValueError, "integrity"):
                paths.verify(root)
            location.write_text(json.dumps({"assets": {"cap": "../outside.cap"}}))
            with self.assertRaisesRegex(ValueError, "escapes"):
                paths.fixture_path("cap", root)


if __name__ == "__main__":
    unittest.main()
