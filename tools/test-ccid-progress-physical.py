#!/usr/bin/env python3
"""Compatibility entrypoint for the relocated test."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).resolve().parents[1] / 'tests/hardware/ccid_progress_physical.py'), run_name="__main__")
