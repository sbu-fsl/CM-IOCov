#!/bin/bash

# Usage: sudo ./cleanup_logs.sh
# Clean ups only, do NOT clean test workloads

# Cleanup script for log files with safe backup

# Get the directory where the script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Parent directory of the script
PARENT_DIR="$SCRIPT_DIR/.."

# Resolve the absolute path of the parent directory
PARENT_DIR="$(cd "$PARENT_DIR" && pwd)"

# echo "Script directory: $SCRIPT_DIR"
# echo "Parent directory: $PARENT_DIR"

BACKUP_DIR="$PARENT_DIR/fsl_log_backups"

# Ignore xfsmonkey_logs/202*-xfsMonkey.log
LOG_FILES="202*-xfsMonkey.log ace/202*-bugWorkloadGen.log outfile* bugs missing others out out_compile stat"

# Get the current timestamp
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Ensure BACKUP_DIR exists, create it if necessary
if [ ! -d "$BACKUP_DIR" ]; then
    echo "Creating backup directory: $BACKUP_DIR"
    mkdir -p "$BACKUP_DIR"
fi

# Process each log file pattern
for FILE_PATTERN in $LOG_FILES; do
    echo "Processing pattern: $FILE_PATTERN"
    
    # Find matching files in PARENT_DIR
    MATCHING_FILES=$(find "$PARENT_DIR" -type f -name "$FILE_PATTERN" 2>/dev/null)
    
    if [ -z "$MATCHING_FILES" ]; then
        echo "No files found for pattern: $FILE_PATTERN"
        continue
    fi

    for FILE in $MATCHING_FILES; do
        # Generate a new name with timestamp for the backup file
        BASENAME=$(basename "$FILE")
        BACKUP_FILE="$BACKUP_DIR/${BASENAME%.*}_$TIMESTAMP.${BASENAME##*.}"

        echo "Backing up $FILE to $BACKUP_FILE"
        
        # Move and rename the file to the backup directory
        mv "$FILE" "$BACKUP_FILE"

        # Verify the move was successful
        if [ -f "$BACKUP_FILE" ]; then
            echo "Successfully backed up and deleted: $FILE"
        else
            echo "Failed to backup $FILE. Skipping deletion."
        fi
    done
done

echo "Log cleanup completed. Backups are stored in $BACKUP_DIR."
