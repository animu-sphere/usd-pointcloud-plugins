"""Compatibility entry point; the packaged implementation lives in acceptance/."""
from pathlib import Path
import runpy

if __name__ == '__main__':
    runpy.run_path(str(Path(__file__).resolve().parent / 'acceptance' / Path(__file__).name), run_name='__main__')
