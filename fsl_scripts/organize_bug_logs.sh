#!/bin/bash

# Args: sudo ./organize_bug_logs.sh -f fs_name -w workload_name -t timestamp -d bug_log_dir
# Example: sudo ./organize_bug_logs.sh -f xfs -w seq1_yf_demo -t 20241007_114200 -d cm_iocov_bugs
# fs_name: file system under test, e.g., btrfs, ext4, xfs
# workload_name: seq1_demo, seq1_yf_demo, seq2
# timestamp: timestamp of the xfsMonkey bug log (timestamp-xfsMonkey.log)
# bug_log_dir: directory containing the CM bug logs, e.g., cm_iocov_bugs, cm_bugs

FS=btrfs
WORKLOAD="seq1_demo"
TIMESTAMP="20241007_114200"
BUG_LOG_DIR="cm_iocov_bugs"

# Parsing arguments 
while [[ $# -gt 0 ]]; do
  case $1 in
    --help)
      echo "Usage: $0 [-f fs_name] [-w workload_name] [-t timestamp] [-d bug_log_dir] [--help]."
      echo "Example: $0 -f btrfs -w seq1_demo -t 20241007_114200 -d cm_iocov_bugs"
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
    -t)
      TIMESTAMP=$2
      shift 2
      ;;
    -d)
      BUG_LOG_DIR=$2
      shift 2
      ;;	  
    *)
      echo "Invalid option: $1"
      exit 1
      ;;
  esac
done

# Change dir to CM root
cd ..

# Create a directory under bug_log_dir for the bug logs
TARGET_DIR="$BUG_LOG_DIR/$FS-$WORKLOAD-$TIMESTAMP"

# Ensure the target directory exists, create it if it doesn't
if [ ! -d "$TARGET_DIR" ]; then
    echo "Creating target directory: $TARGET_DIR"
    mkdir -p "$TARGET_DIR"
fi

# Logs and workloads to move
# Ignore xfsmonkey_logs/202*-xfsMonkey.log
LOG_FILES="$TIMESTAMP-xfsMonkey.log outfile* bugs missing others out out_compile stat ace/202*-bugWorkloadGen.log"
LOGS_DIRS="diff_results"
WORKLOAD_FILES="code/tests/$WORKLOAD"

COMBINED_FILES="$LOG_FILES $LOGS_DIRS $WORKLOAD_FILES"

# Move each file or directory to the target directory
for ITEM in $COMBINED_FILES; do
    if [ -e "$ITEM" ]; then
        echo "Moving $ITEM to $TARGET_DIR"
        mv "$ITEM" "$TARGET_DIR"
    else
        echo "Warning: $ITEM does not exist and will be skipped."
    fi
done

# Summary
echo "All Log/Workload files/directories have been processed."
