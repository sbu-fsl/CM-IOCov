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
