# CrashMonkey and Ace #

CM-IOCov is an enhanced crash-consistency testing tool based on the [Original CrashMonkey](https://www.cs.utexas.edu/~vijay/papers/osdi18-crashmonkey.pdf) framework. It extends CrashMonkey by improving syscall argument diversity (i.e., input coverage) and integrating the IOCov framework, which introduces input and output coverage as new metrics for evaluating the effectiveness of file system testing. This updated version supports modern Linux kernels version 6.12, with substantial updates to the kernel modules and testing infrastructure.

The updated CM-IOCov and Ace tools have uncovered several long-standing crash-consistency bugs in widely used file systems such as Btrfs on Linux 6.12. These tools work out-of-the-box with any POSIX-compliant file system—no modifications to file system code are required.

### CrashMonkey ###
CrashMonkey is a file-system agnostic record-replay-and-test framework. Unlike existing tools like dm-log-writes which require a manual checker script, CrashMonkey automatically tests for data and metadata consistency of persisted files. CrashMonkey needs only one input to run - the workload to be tested. We have described the rules for writing a workload for CrashMonkey [here](docs/workload.md). More details about the internals of CrashMonkey can be found [here](docs/CrashMonkey.md).


### Automatic Crash Explorer (Ace) ###
Ace is an automatic workload generator, that exhaustively generates sequences of file-system operations (workloads), given certain bounds. Ace consists of a workload synthesizer that generates workloads in a high-level language which we call J-lang. A CrashMonkey adapter, that we built, converts these workloads into C++ test files that CrashMonkey can work with. More details on the current bounds imposed by Ace and guidelines on workload generation can be found [here](docs/Ace.md).

CrashMonkey and Ace can be used out of the box on any Linux file system that implements the POSIX API. The tools have been tested and verified to work with Btrfs on Linux kernel version 6.12.

### Results ###
We tested the Btrfs file system on Linux kernel version 6.12. Our enhanced tool, CM-IOCov, identified 74.1% more test failures (potential crash-consistency bugs) compared to the original CrashMonkey. These results demonstrate the effectiveness of input coverage–driven testing in uncovering bugs that traditional approaches may miss.



## Table Of Contents ##
1. [Setup](#setup)
2. [Push Button Testing for Seq-1 Workloads](#push-button-testing-for-seq-1-workloads)
3. [Tutorial on Workload Generation and Testing](#tutorial)

## Setup ##

Here is a checklist of dependencies to get CrashMonkey and Ace up and running on your system.
* To get started, you’ll need a Linux machine. We recommend setting up an Ubuntu 22.04 VM with Linux kernel version 6.12. For running large-scale tests, it’s best to allocate at least 500GB of disk space and 64GB of RAM.
* To manually install Linux kernel version 6.12, you can follow steps similar to those outlined in [this guide](https://www.cyberciti.biz/tips/compiling-linux-kernel-26.html). Although the guide is for an earlier kernel version, the instructions remain largely applicable to 6.12.
* Before compiling the kernel, disable the following security-related options to avoid compatibility issues:

    <pre><code>
    scripts/config --disable CONFIG_MODULE_SIG
    scripts/config --set-str CONFIG_SYSTEM_TRUSTED_KEYS ""
    scripts/config --set-str CONFIG_SYSTEM_REVOCATION_KEYS ""
    scripts/config --disable CONFIG_MODULE_SIG_FORCE
    scripts/config --disable CONFIG_SECURITY
    scripts/config --disable CONFIG_SECURITY_SELINUX
    scripts/config --disable CONFIG_SECURITY_APPARMOR
    scripts/config --disable CONFIG_SECURITY_YAMA
    scripts/config --disable CONFIG_KEYS
    </code></pre>

   After making the changes, update the kernel configuration and compile.

* Install the necessary system packages and Python libraries:

  `apt-get install git make gcc g++ libattr1-dev btrfs-progs f2fs-tools xfsprogs libelf-dev python3 python3-pip`

  `python3 -m pip install progress progressbar`

* Since you manually compiled the kernel, the headers are not available through the standard Ubuntu repositories. You must generate and install them manually:

     *** Navigate to kernel source code ***

	`cd /home/user/linux-6.12` 

    `sudo make headers_install INSTALL_HDR_PATH=/usr/src/linux-headers-6.12.0`

    This installs only the user-space headers (i.e., the include/ directory). These are sufficient for compiling user-level programs but not for building kernel modules. To enable kernel module compilation, create a symbolic link from the full kernel source tree:

    `sudo ln -s /home/user/linux-6.12 /lib/modules/6.12.0/build`

    This ensures that tools looking for kernel build headers can locate the complete source tree.
  
*  Clone the repository.

    `git clone https://github.com/sbu-fsl/CM-IOCov.git`

* Compile CrashMonkey's test harness, kernel modules and the test suite of seq-1 workloads (workloads with 1 file-system operation). The initial compile should take a minute or less.

  `cd crashmonkey; make -j4`

* Create a directory for the test harness to mount devices to.

  `mkdir /mnt/snapshot`


## Push Button Testing for Seq-1 Workloads ##

This repository contains a pre-generated suite of 328 seq-1 workloads (workloads with 1 file-system operation) [here](code/tests/seq1/). Once you have [set up](#setup) CrashMonkey on your machine (or VM), you can simply run :

```python
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 204800 -u build/tests/seq1/ > outfile
```

Sit back and relax. This is going to take a few minutes to complete if run on a single machine. This will run all the 328 tests of seq-1 on a `btrfs` file system `200MB` in size. The bug reports can be found in the folder `diff_results`. The workloads are named j-lang<1-328>, and, if any of these resulted in a bug, you will see a bug report with the same name as that of the workload, describing the difference between the expected and actual state.

## Tutorial ##
This tutorial walks you through the workflow of workload generation to testing, using a small bounded space of seq-1 workloads. Generating and running the tests in this tutorial will take less than 2 minutes.

1. **Select Bounds** :
Let us generate workloads of sequence length 1, and test only two file-system operations, `link` and `fallocate`. Our reduced file set consists of just two files.

    ```python
    FileOptions = ['foo']
    SecondFileOptions = ['A/bar']
    ```

    The link and fallocate system calls pick file arguments from the above list. Additionally fallocate allows several modes including `ZERO_RANGE`, `PUNCH_HOLE` etc. We pick one of the modes to bound the space here.

    ```python
    FallocOptions = ['FALLOC_FL_ZERO_RANGE|FALLOC_FL_KEEP_SIZE']
    ```

    The fallocate system call also requires offset and length parameters which are chosen to be one of the following.
    ```python
    WriteOptions = ['append', 'overlap_unaligned_start', 'overlap_extend']
    ```

    All these options are configurable in the [ace script](ace/ace.py).


2. **Generate Workloads** :
To generate workloads conforming to the above mentioned bounds, run the following command in the `ace` directory  :
    ```python
    cd ace
    python ace.py -l 1 -n False -d True
    ```

    `-l` flag sets the length of the sequence to 1, `-n` is used to indicate if we want to include a nested directory to the file set and `-d` indicates the demo workload set, which appropriately sets the above mentioned bounds on system calls and their parameters.

    This generates about 9 workloads in about a second. You will find the generated workloads at `code/tests/seq1_demo`. Additionally, you can find the J-lang equivalent of these test files at `code/tests/seq1_demo/j-lang-files`

3. **Compile Workloads** : In order to compile the test workloads into `.so` files to be run by CrashMonkey

    1. Copy the generated test files into `generated_workloads` directory.
    ```
    cd ..
    cp code/tests/seq1_demo/j-lang*.cpp code/tests/generated_workloads/
    ```
    2. Compile the new tests.
    ```
    make gentests
    ```

    This will compile all the new tests and place the `.so` files at `build/tests/generated_workloads`

4. **Run** : Now its time to test all these workloads using CrashMonkey. Run the xfsMonkey script, which simply invokes CrashMonkey in a loop, testing one workload at a time.

    For example, let's run the generated tests on the `btrfs` file system, on a `200MB` image.

    ```python
    python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t btrfs -e 204800 -u build/tests/generated_workloads/ > outfile
    ```

5. **Bug Reports** : The generated bug reports can be found at `diff_results`. If the test file "x" triggered a bug, you will find a bug report with the same name in this directory.

    For example, j-lang22937.cpp will result in a crash-consistency bug on btrfs, as on kernel 6.12 . The corresponding bug report will be as follows.

```    
DIFF: Content Mismatch /foo

/mnt/snapshot/foo:
---Directory Atrributes---
Name   : 
Inode  : 18446744073709551615
Offset : -1
Length : 65535
Type   : �
---File Stat Atrributes---
Inode     : 257
TotalSize : 65537
BlockSize : 4096
#Blocks   : 136
#HardLinks: 1
Mode      : 33188
User ID   : 0
Group ID  : 0
Device ID : 0
RootDev ID: 49


/mnt/cow_ram_snapshot3_0/foo:
---Directory Atrributes---
Name   : 
Inode  : 18446744073709551615
Offset : -1
Length : 65535
Type   : �
---File Stat Atrributes---
Inode     : 257
TotalSize : 65537
BlockSize : 4096
#Blocks   : 2056
#HardLinks: 1
Mode      : 33188
User ID   : 0
Group ID  : 0
Device ID : 0
RootDev ID: 51
```
