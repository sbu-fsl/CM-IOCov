# CM-IOCov: an extension of CrashMonkey with improved input coverage

## Overview

This is the CM-IOCov repository, which is an extension of
[CrashMonkey](https://github.com/utsaslab/crashmonkey) with improved input
coverage. The paper of CM-IOCov and input/output testing for file systems is
published in ACM SYSTOR 2025, and the PDF version is
[here](https://www.fsl.cs.stonybrook.edu/docs/mcfs/systor25cmiocov.pdf).

CM-IOCov has a dedicated input driver to generate inputs (file system call
arguments) and does not use the original way of CrashMonkey selecting inputs, to
support diverse inputs for generating test workloads. CM-IOCov reuses all other
parts of CrashMonkey for crash consistency bug detection such as crash
generation, oracle generation, and auto checker. For more information about
CrashMonkey, please refer to the
[CrashMonkey README](README-CRASHMONKEY.md) and paper.

This repository is cloned from the original CrashMonkey repo and makes changes
to support CM-IOCov. For the master branch here, the implementation is still the
original CrashMonkey, please see the [Branches](#branches) for implementations
of CM-IOCov.

The associated repository with CM-IOCov is IOCov (https://github.com/sbu-fsl/IOCov), which measures
input and output coverage for file system testing tools.

---

## Branches



## Usage



## Citation 


## Contact

