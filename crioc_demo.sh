#!/bin/bash

# exit immediately on any failure
# set -e

# print all executed commands (debug mode)
# set -x 

# CrashMonkey-IOCov Demo Script extended from demo.sh with handling more parameters
# Usage: sudo ./crioc_demo.sh [-f fs_name] [-w workload_name] [-s dev_sz_kb] [-l ace_sequence_len] [-n ace_nested] [-d ace_demo] [--help]
# Default: sudo ./crioc_demo.sh -f btrfs -w seq1_demo -s 204800 -l 1 -n False -d True
# Seq2 Example: sudo ./crioc_demo.sh -f btrfs -w seq2 -s 204800 -l 2 -n False -d False

FS=btrfs
WORKLOAD=seq1_demo
DEVSZKB=204800
ACE_SEQ_LEN=1
ACE_NESTED=False
ACE_DEMO=True

# Parsing arguments 
while [[ $# -gt 0 ]]; do
  case $1 in
    --help)
      echo "Usage: $0 [-f fs_name] [-w workload_name] [-s dev_sz_kb] [-l ace_sequence_len] [-n ace_nested] [-d ace_demo] [--help]."
	  echo "Example: $0 -f btrfs -w seq1_demo -s 204800 -l 1 -n False -d True"
      exit 0
      ;;
    -f)
      FS=$2
      shift 2
      ;;
    -w)
      WORKLOAD=$2
      shift 2
      ;;
    -s)
      DEVSZKB=$2
      shift 2
      ;;
    -l)
      ACE_SEQ_LEN=$2
      shift 2
      ;;
    -n)
      ACE_NESTED=$2
      shift 2
      ;;
    -d)
      ACE_DEMO=$2
      shift 2
      ;;	  
    *)
      echo "Invalid option: $1"
      exit 1
      ;;
  esac
done

echo "Running CrashMonkey-IOCov Demo with the following parameters:"
echo "Filesystem: $FS"
echo "Workload: $WORKLOAD"
echo "Device Size: $DEVSZKB KB"
echo "ACE Squence Length: $ACE_SEQ_LEN"
echo "ACE Nested: $ACE_NESTED"
echo "ACE Demo: $ACE_DEMO"
echo "----------------------------------"

WORKLOAD_DIR="code/tests/$WORKLOAD"
TARGET_DIR="code/tests/generated_workloads"
TARGET_BUILD_DIR="build/tests/generated_workloads/"
REPORT_DIR="diff_results"

bold=$(tput bold)
reset=$(tput sgr0)

# Starting workload generation..
# Let's use the restricted bounds defined by -d flag in the workload generator
echo "Cleaning up the target workload directory"
if [ -d "$WORKLOAD_DIR" ]; then
    mv "$WORKLOAD_DIR" "${WORKLOAD_DIR}_$(date +%Y%m%d_%H%M%S)"
fi

echo "Starting workload generation.."
# Change to ace directory
cd ace
start=`date +%s.%3N`
python ace.py -l $ACE_SEQ_LEN -n $ACE_NESTED -d $ACE_DEMO

end_gen=`date +%s.%3N`
# Now let's compile the generated tests
cd -

# Before starting compilation, let's cleanup the target directories, just to be sure we'll run only the demo workloads. Also, copy generated workloads to TARGET_DIR
if [ -d "$TARGET_DIR" ]; then
    mv "$TARGET_DIR" "${TARGET_DIR}_$(date +%Y%m%d_%H%M%S)"
fi
mkdir -p "$TARGET_DIR"

# To fix "cp: argument list too long" error, use find and xargs
# cp $WORKLOAD_DIR/j-lang*.cpp $TARGET_DIR/
find "$WORKLOAD_DIR" -maxdepth 1 -name "j-lang*.cpp" -print0 | xargs -0 cp -t "$TARGET_DIR" || { echo "Workload CPP files copy failed. Exiting."; exit 1; }

echo "Workload generation complete. Compiling workloads.."
make gentests > out_compile 2>&1

end_compile=`date +%s.%3N`

# The workloads are now compiled and placed under TARGET_BUILD_DIR. 
# Run the workloads

echo 0 > bugs
echo 0 > stat
echo 0 > missing
echo 0 > others

echo -e "\nCompleted compilation. Testing workloads on $FS.."
if [ -d "$REPORT_DIR" ]; then
    mv "$REPORT_DIR" "${REPORT_DIR}_$(date +%Y%m%d_%H%M%S)"
fi
# Yifei: change 102400 to 204800, as it requires at least 200MB disk space
python xfsMonkey.py -f /dev/sda -d /dev/cow_ram0 -t $FS -e $DEVSZKB -u $TARGET_BUILD_DIR

end=`date +%s.%3N`
run_time=$( echo "$end - $start" | bc -l)

echo -e "====================Summary=====================\n"
echo -ne "${bold}"
printf "Demo Completed in %.2f seconds\n" $run_time
echo -ne "${reset}"
gen_time=$( echo "$end_gen - $start" | bc -l )
compile_time=$( echo "$end_compile - $end_gen" | bc -l )
test_time=$( echo "$end - $end_compile" | bc -l )

echo -e "----------------------------------"
printf "Generation   \t\t %.2f s" $gen_time
printf "\nCompilation\t\t %.2f s" $compile_time
printf "\nTesting    \t\t %.2f s" $test_time 
echo -ne "${bold}"
printf "\n\nTotal bugs   \t\t %d " `cat bugs`
echo -ne "${reset}"
echo -e "\n----------------------------------"
printf "Metadata Mismatch\t %d " `cat stat`
printf "\nFile Missing    \t %d " `cat missing` 
printf "\nData Mismatch    \t %d " `cat others` 
echo -e "\n----------------------------------\n"
echo -e "See complete bug reports at diff-results/\n"
