#!/usr/bin/env python3
# R8 consolidates R7 info checks with production scheduler/lifecycle tests.
import pathlib, runpy
runpy.run_path(str(pathlib.Path(__file__).with_name('run-sync-check.py')),run_name='__main__')
