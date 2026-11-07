"""Import the canonical fixture resolver for directly executed test scripts."""
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[2]
_spec = importlib.util.spec_from_file_location("validation_fixture_paths", ROOT / "tests/fixtures/paths.py")
_module = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(_module)
fixture_path = _module.fixture_path
manifest = _module.manifest
verify = _module.verify
