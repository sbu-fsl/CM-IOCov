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

This repository provides multiple branches to distinguish between the original
CrashMonkey and CM-IOCov, as well as kernel version support:

- [**master**](https://github.com/sbu-fsl/CM-IOCov/tree/master)  
  The original [CrashMonkey](https://github.com/utsaslab/crashmonkey) codebase
  without CM-IOCov modifications. CrashMonkey supports Linux kernels up to
  **5.6**.

- [**cmiocov-base**](https://github.com/sbu-fsl/CM-IOCov/tree/cmiocov-base)  
  Baseline CM-IOCov branch built on top of CrashMonkey, supporting the same
  kernel versions as the original CrashMonkey (up to Linux 5.6).

- [**crashmonkey-linux-6.12**](https://github.com/sbu-fsl/CM-IOCov/tree/crashmonkey-linux-6.12)  
  CrashMonkey adapted to support Linux kernel **6.12**, extending kernel
  compatibility beyond 5.6.

- [**cm-iocov-linux-6.12**](https://github.com/sbu-fsl/CM-IOCov/tree/cm-iocov-linux-6.12)  
  CM-IOCov adapted to support Linux kernel **6.12**, with extended input
  coverage functionality in addition to the newer kernel support.

---

## Usage

The usage of CM-IOCov is identical to CrashMonkey. Please follow the
instructions in the [CrashMonkey README](README-CRASHMONKEY.md). First, check
out the respective branch that matches your kernel version (`cmiocov-base` 
for kernels up to 5.6, `cm-iocov-linux-6.12` for Linux 6.12).
Then build the tests and run them using the same workflow as CrashMonkey to
execute workloads and check for crash consistency bugs.

## Citation 

```
@INPROCEEDINGS{systor25cmiocov,
  TITLE =        "Enhanced File System Testing through Input and Output Coverage",
  AUTHOR =       "Yifei Liu and Geoff Kuenning and Md. Kamal Parvez and Scott Smolka and Erez Zadok",
  BOOKTITLE =    "Proceedings of the 18th ACM International Systems and Storage Conference (SYSTOR '25)",
  ADDRESS =      "Virtual Event",
  MONTH =        "September",
  YEAR =         "2025",
  PUBLISHER =    "ACM",
}
```

---

## Contact

For any question, please feel free to contact Yifei Liu ([yifeliu@cs.stonybrook.edu](mailto:yifeliu@cs.stonybrook.edu))
and Erez Zadok ([ezk@cs.stonybrook.edu](mailto:ezk@cs.stonybrook.edu)).
Alternatively, you can create an issue in this repository.
