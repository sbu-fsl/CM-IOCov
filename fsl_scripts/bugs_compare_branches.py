#!/usr/bin/env python3
import os

cm_iocov_path="/crashmonkey-iocov-part/cm-iocov-2025-0120/crashmonkey-iocov"
original_cm_Path="/crashmonkey-part/unmodified-crashmonkey-2025-0120/crashmonkey-iocov"

diff_dir = "diff_results"

dir_cm_iocov = os.path.join(cm_iocov_path, diff_dir)
dir_original_cm = os.path.join(original_cm_Path, diff_dir)

files_cm_iocov = set(f for f in os.listdir(dir_cm_iocov) if os.path.isfile(os.path.join(dir_cm_iocov, f)))
files_original_cm = set(f for f in os.listdir(dir_original_cm) if os.path.isfile(os.path.join(dir_original_cm, f)))

cm_iocov_only = list(files_cm_iocov - files_original_cm)
original_cm_only = list(files_original_cm - files_cm_iocov)

print("Only in cm_iocov:", cm_iocov_only)
print("Only in original_cm:", original_cm_only)
