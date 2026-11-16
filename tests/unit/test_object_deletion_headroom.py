import importlib.util
import tempfile
import unittest
from pathlib import Path

SCRIPT = Path(__file__).resolve().parents[2] / "tools/check-object-deletion-headroom.py"
SPEC = importlib.util.spec_from_file_location("headroom", SCRIPT)
HEADROOM = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(HEADROOM)


class HeadroomTests(unittest.TestCase):
    def inspect(self, content):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "serial.log"
            path.write_text(content)
            return HEADROOM.inspect(path)

    def test_aggregates_peak_files_and_bytes(self):
        samples, files, used, capacity = self.inspect(
            "OBJECT_DELETION:PRE_CLEANUP reason=applet files=2 bytes=10/100\n"
            "OBJECT_DELETION:PRE_CLEANUP reason=automatic files=8 bytes=89/100\n"
        )
        self.assertEqual((samples, files, used, capacity), (2, 8, 89, 100))

    def test_missing_samples_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "no OBJECT_DELETION"):
            self.inspect("BOOT:READY\n")

    def test_invalid_capacity_fails_closed(self):
        with self.assertRaisesRegex(ValueError, "malformed"):
            self.inspect("OBJECT_DELETION:PRE_CLEANUP files=2 bytes=10/0\n")

    def test_mixed_capacities_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "multiple storage capacities"):
            self.inspect(
                "OBJECT_DELETION:PRE_CLEANUP files=2 bytes=10/100\n"
                "OBJECT_DELETION:PRE_CLEANUP files=3 bytes=20/200\n"
            )

    def test_limit_is_strictly_above_ninety_percent(self):
        self.assertFalse(HEADROOM.exceeds_limit(90, 100))
        self.assertTrue(HEADROOM.exceeds_limit(91, 100))


if __name__ == "__main__":
    unittest.main()
