import subprocess
import os
import shutil
import re
import tempfile
import shlex
import pandas as pd



remote_host_iocov = "root@130.245.126.186"
remote_host_orig = "root@130.245.126.228"
ssh_port = "130"
remote_base_path_iocov= "/home/kamal/crashmonkey-iocov"
remote_base_path_orig = "/mnt/disk2/crashmonkey-iocov"
log_pattern = "output*.log"

report_file = "bug_comparison_results.xlsx"
sheet_iocov_details = "IOCov Bug Details"
sheet_iocov_summary = "IOCov Summary"
sheet_orig_details = "Original Bug Details"
sheet_orig_summary = "Original Summary"
sheet_iocov_unique = "IOCov Unique Bugs"
sheet_orig_unique = "Original Unique Bugs"

iocov_res_dir = "Bugs_Passed_Original_Failed_IOCov"
orig_res_dir = "Bugs_Passed_IOCov_Failed_Original"

testfile_dir = "Test_Workload"
bug_file_dir = "Bug_Details"

exclude_ops = ('fsync', 'sync', 'close')



cols_details = [
    "No.",
    "File System under Test",
    "Linux Kernel Version",
    "Test Workload Name",
    "Log Msg",
    "Bug Consequence",
    "Operation Sequence",
    "Found in CM-IOCov",
    "Found in the original CrashMonkey"
]

# Create empty DataFrame with the correct columns
df_iocov = pd.DataFrame(columns=cols_details).astype({
    "No.": int,
    "File System under Test": str,
    "Linux Kernel Version": str,
    "Test Workload Name": str,
    "Log Msg": str,
    "Bug Consequence": str,
    "Operation Sequence": str,
    "Found in CM-IOCov": str,
    "Found in the original CrashMonkey": str
})
df_orig = pd.DataFrame(columns=cols_details).astype({
    "No.": int,
    "File System under Test": str,
    "Linux Kernel Version": str,
    "Test Workload Name": str,
    "Log Msg": str,
    "Bug Consequence": str,
    "Operation Sequence": str,
    "Found in CM-IOCov": str,
    "Found in the original CrashMonkey": str
})




consequence_list = {
    "block_lost": "Allocated blocks lost after fsync()",
    "content_mismatch": "File content didn't match after fsync()",
    "block_missing_rename": "Data block missing after rename()",
    "rename_not_perssist": "Rename not persisted by fsync()",
    "incorrect_link": "Incorrect number of file hard links after fsync()"
}




def fetch_and_merge_diff_results(remote_host, remote_base, local_dir):
    os.makedirs(local_dir, exist_ok=True)

    # Find all files under diff_results*
    find_cmd = [
        "ssh", "-p", ssh_port, remote_host,
        f"find {remote_base} -type f -path '*/diff_results*/*'"
    ]
    result = subprocess.run(find_cmd, check=True, capture_output=True, text=True)
    file_paths = result.stdout.strip().splitlines()

    if not file_paths:
        print("No diff_results files found on remote.")
        return

    # Prepare remote tar command to flatten the files into /tmp/flattened
    # and then archive them from that temp directory
    tar_cmd = " && ".join([
        "rm -rf /tmp/flattened",                   # clean temp dir
        "mkdir -p /tmp/flattened",
        *[
            f"cp {shlex.quote(f)} /tmp/flattened/{shlex.quote(os.path.basename(f))}"
            for f in file_paths
        ],
        "tar -cf - -C /tmp/flattened ."
    ])

    # Stream tarball to local
    with tempfile.NamedTemporaryFile(delete=False, suffix=".tar") as tmp_tar:
        tarball_path = tmp_tar.name

    with open(tarball_path, "wb") as f:
        subprocess.run(["ssh", "-p", ssh_port, remote_host, tar_cmd], check=True, stdout=f)

    # Extract locally
    subprocess.run(["tar", "-xf", tarball_path, "-C", local_dir], check=True)

    os.remove(tarball_path)


def fetch_and_merge_remote_logs(remote_host, remote_base, local_dir, logfile):
    """
    SSH into `remote_host`, find all files matching output*.log in remote_base,
    and merge their contents into one local file named `logfile` inside local_dir.
    """
    # Ensure local folder exists
    os.makedirs(local_dir, exist_ok=True)
    logfile_path = os.path.join( local_dir, logfile)

    # Find remote log files
    find_cmd = [
        "ssh", "-p", ssh_port, remote_host,
        f"find {remote_base} -maxdepth 1 -type f -name '{log_pattern}'"
    ]
    result = subprocess.run(find_cmd, capture_output=True, text=True, check=True)
    remote_logs = [line.strip() for line in result.stdout.splitlines() if line.strip()]

    if not remote_logs:
        print("🔍 No output*.log files found on remote.")
        return

    # Merge them locally by cat’ing over SSH
    with open(logfile_path, "w") as outfile:
        for remote_log in remote_logs:
            print(f"🛰  Fetching {remote_log} ...")
            cat_cmd = ["ssh", "-p", ssh_port, remote_host, f"cat {remote_log}"]
            proc = subprocess.Popen(cat_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            stdout, stderr = proc.communicate()

            if proc.returncode != 0:
                print(f"⚠️  Failed to read {remote_log}: {stderr.decode().strip()}")
                continue

            outfile.write(stdout.decode())
            # optional separator between logs
            outfile.write("\n")

    print(f"✅ Merged {len(remote_logs)} log(s) into {logfile_path}")


def add_log(df, count, workload_name, log_msg):

    # Remove control characters (except newline and tab)
    sanitized_log = re.sub(r"[\x00-\x08\x0b\x0c\x0e-\x1f]", "", log_msg)
    # Remove pseudo-ANSI tags like [31m, [1m, etc.
    sanitized_log = re.sub(r'\[\d{0,3}(;\d{1,3})*m', '', sanitized_log)

    df.loc[len(df)] = {
        "No.": count,
        "File System under Test": "Btrfs",
        "Linux Kernel Version": "6.12",
        "Test Workload Name": workload_name,
        "Log Msg": sanitized_log
    }



def analyze_test_results(df, diff_results_dir, log_a, log_b):
    found_b = []
    not_found_b = []
    passed_b = []
    failed_b = []
    could_not_run_b = []

    # Load log file lines into memory
    with open(log_a, "r", encoding="utf-8", errors="ignore") as log_file:
        log_lines_a = log_file.readlines()

    with open(log_b, "r", encoding="utf-8", errors="ignore") as log_file:
        log_lines_b = log_file.readlines()

    file_count = 0
    target_count = 0


    for filename in os.listdir(diff_results_dir):

        # if file_count == 10:
        #     break

        file_count += 1
        match_found = False
        escaped_filename = re.escape(filename)  # Handle special chars in filename
        pattern = re.compile(rf"\b{escaped_filename}\b")  # Exact match

        for idx, line_b in enumerate(log_lines_b):
            if pattern.search(line_b):
                found_b.append(filename)
                match_found = True

                # Check for "Passed" or "Failed" in the same line
                if "Passed" in line_b or "Could not run" in line_b:

                    if "Passed" in line_b:
                        passed_b.append(filename)
                    elif "Could not run" in line_b:
                        could_not_run_b.append(filename)

                    target_count += 1
                    log_msg = ""

                    for jdx, line_a in enumerate(log_lines_a):
                        if pattern.search(line_a):
                            block = []
                            for sub2 in log_lines_a[jdx+1:]:
                                if "Running test" in sub2 and sub2 != line_a:
                                    break
                                block.append(sub2)
                            log_msg = "".join(block)
                            break  
                    add_log(df, target_count, filename, log_msg)

                elif "Failed" in line_b:
                    failed_b.append(filename)

                break

        if not match_found:
            not_found_b.append(filename)

    return file_count, found_b, not_found_b, passed_b, failed_b, could_not_run_b



def copy_bug_details(filenames, search_dir, target_dir):
    os.makedirs(target_dir, exist_ok=True)

    for name in filenames:
        found = False
        for root, dirs, files in os.walk(search_dir):
            if name in files:
                src_path = os.path.join(root, name)
                dst_path = os.path.join(target_dir, name)
                shutil.copy2(src_path, dst_path)
                found = True
                break 
        if not found:
            print(f"❌ Not found: {name}")



def fetch_workload(remote_host, remote_dir, filenames, local_dir):
    local_dir = os.path.expanduser(local_dir)
    os.makedirs(local_dir, exist_ok=True)

    for name in filenames:
        remote_path = f"{remote_host}:{remote_dir}/{name}"
        # print(f"Copying {name}…")
        try:
            subprocess.run(
                ["scp", "-P", ssh_port, remote_path, local_dir],
                check=True
            )
        except subprocess.CalledProcessError as e:
            print(f"  ❌ failed to copy {name}: {e}")


def compare_bugs(df, diff_res_path, log_a, log_b, res_dir):

    cur_dir = os.getcwd()
    cur_dir = os.path.join(cur_dir, res_dir)
    os.makedirs(cur_dir, exist_ok=True)

    file_count, found_b, not_found_b, passed_b, failed_b, could_not_run_b = analyze_test_results(df, diff_res_path, log_a, log_b)
    
    # Considering passed and could not run testcases as target bug
    considered_bug = passed_b + could_not_run_b

    # Copy the bug details files for reporting
    copy_bug_details(considered_bug, diff_res_path, os.path.join(cur_dir, "Bug_Details"))

    # Get test workload
    # cpp_path = "code/tests/generated_workloads"
    workload_dir = "code/tests/seq2/j-lang-files/"


    workload_path = os.path.join(remote_base_path_iocov, workload_dir)
    local_path = os.path.join(cur_dir, "Test_Workload")
    fetch_workload(remote_host_iocov, workload_path, considered_bug, local_path)

    return file_count, found_b, not_found_b, passed_b, failed_b, could_not_run_b


def extract_operations(file_path, target_name):
    operations = []
    run_section = False
    run_lines = []

    if not os.path.exists(file_path):
        print(f"File not found: {file_path}")
        return operations

    with open(file_path, 'r') as f:
        for line in f:
            stripped = line.strip()

            # Detect start of # run section
            if stripped.lower().startswith("# run"):
                run_section = True
                continue

            # Stop if next section starts (e.g., # declare, # setup)
            if run_section and stripped.startswith("#"):
                break
            
            if run_section:
                if stripped.startswith("#"):  # next section
                    break
                run_lines.append(stripped)


    # Top-down search
    for line in run_lines:
        if " " + target_name in line:
            syscall = line.split(" ")[0]
            operations.append(syscall)

    # If no operations found, do bottom-up rename tracing
    if not operations:
        reversed_lines = list(reversed(run_lines))

        for line in reversed_lines:
            tokens = line.split()
            if not tokens:
                continue
            if tokens[0] == "rename" and len(tokens) >= 3:
                # replacing new with old
                target_name = target_name.replace(tokens[2], tokens[1])
                operations.append(tokens[0]) # rename syscall is added
                continue

            if " " + target_name in line:
                operations.append(tokens[0])
            
        operations.reverse() # as searched bottom-up

    return operations


def make_bug_report(df, parent_dir):
    bug_file_par = os.path.join(parent_dir, bug_file_dir)

    for idx, row in df.iterrows():
        # if idx == 5:
        #     break

        # print(row["Log Msg"])
        log_data = row["Log Msg"]
        test_name_data = row["Test Workload Name"]
        op_field = "Operation Sequence"
        cons_field = "Bug Consequence"

        # print(test_name_data)

        # Getting log msg from excel. if not found, getting it from bug details files
        log_data = str(log_data) if pd.notna(log_data) else ""
        if not log_data:
            if os.path.isfile(os.path.join(bug_file_par, test_name_data)):
                with open(os.path.join(bug_file_par, test_name_data), 'r', encoding='utf-8', errors='ignore') as f:
                    log_data = f.read()

        # Searching the relevant directory/file name
        match = re.search(r'(?:Content Mismatch(?: of file)?|Failed stating the file)\s+(\S+)', log_data)
        if match:
            target_file = match.group(1).replace("mnt/snapshot", "")
            target_file = target_file.replace("/", "")

        # print(target_file)

        # Extracting the relevant operations
        testfile_path = os.path.join(os.path.join(parent_dir, testfile_dir), test_name_data)
        operations = extract_operations(testfile_path, target_file)
        operations = [op for op in operations if op not in exclude_ops]
        # print(operations)

        # Save the operations in excel
        df.loc[idx, op_field] = ", ".join(operations)


        # Analyze consequences
        if "Link Count" in log_data:
            df.loc[idx, cons_field] = consequence_list["incorrect_link"]
        elif "Block Count" in log_data:
            df.loc[idx, cons_field] = consequence_list["block_lost"]
        elif "compare_file_contents failed" in log_data:
            df.loc[idx, cons_field] = consequence_list["content_mismatch"]
        elif "Failed stating the file" and "rename" in operations:
            df.loc[idx, cons_field] = consequence_list["rename_not_perssist"]
        elif "DIFF: Content Mismatch" in log_data:
            df.loc[idx, cons_field] = consequence_list["block_lost"]



def generate_all_bugs():

    cur_dir = os.getcwd()

    # Get all test failure files from IOCov CrashMonkey
    diff_res_path_iocov = os.path.join(cur_dir, "diff_results_iocov")
    fetch_and_merge_diff_results(remote_host_iocov, remote_base_path_iocov, diff_res_path_iocov)
    
    # Get all test failure files from Original CrashMonkey
    diff_res_path_orig = os.path.join(cur_dir, "diff_results_original")
    fetch_and_merge_diff_results(remote_host_orig, remote_base_path_orig, diff_res_path_orig)

    # Get all logs from IOCov CrashMonkey and merge them in one log file
    log_iocov = "output_log_iocov.log"
    fetch_and_merge_remote_logs(remote_host_iocov, remote_base_path_iocov, cur_dir, log_iocov)
    
    # Get logs from master branch and merge them in one log file
    log_orig = "output_log_original.log"
    fetch_and_merge_remote_logs(remote_host_orig, remote_base_path_orig, cur_dir, log_orig)

    # Analyze Bugs from IOCov CrashMonkey
    file_count, found_b, not_found_b, passed_b, failed_b, could_not_run_b = compare_bugs(df_iocov, diff_res_path_iocov, log_iocov, log_orig, iocov_res_dir)

    df_iocov['Found in CM-IOCov'] = 'Yes'
    df_iocov['Found in the original CrashMonkey'] = 'No'

    # Save to Excel
    with pd.ExcelWriter(report_file) as writer:
        df_iocov.to_excel(writer, sheet_name=sheet_iocov_details, index=False)
        
        summary_df = pd.DataFrame([{
            "IOCov Test Failures": file_count,
            "Among them Master Run": len(found_b),
            "Master didn't Run Yet": len(not_found_b),
            "Master Passed": len(passed_b),
            "Both Failed": len(failed_b),
            "Master Could Not Run": len(could_not_run_b)
        }])
        
        summary_df.to_excel(writer, sheet_name=sheet_iocov_summary, index=False)

    # # Analyze Bugs from Original CrashMonkey
    file_count, found_b, not_found_b, passed_b, failed_b, could_not_run_b = compare_bugs(df_orig, diff_res_path_orig, log_orig, log_iocov, orig_res_dir)

    df_orig['Found in CM-IOCov'] = 'No'
    df_orig['Found in the original CrashMonkey'] = 'Yes'

    # print(df_orig.head())
    # Save to Excel
    with pd.ExcelWriter(report_file, mode="a", if_sheet_exists='replace', engine='openpyxl') as writer:
        df_orig.to_excel(writer, sheet_name=sheet_orig_details, index=False)
        
        summary_df = pd.DataFrame([{
            "Original Test Failures": file_count,
            "Among them IOCov Run": len(found_b),
            "IOCov didn't Run Yet": len(not_found_b),
            "IOCov Passed": len(passed_b),
            "Both Failed": len(failed_b),
            "IOCov Could Not Run": len(could_not_run_b)
        }])
        
        summary_df.to_excel(writer, sheet_name=sheet_orig_summary, index=False)

    # Prepare Bug Report for IOCov
    make_bug_report(df_iocov, os.path.join(cur_dir, iocov_res_dir))
    with pd.ExcelWriter(report_file, mode="a", engine="openpyxl", if_sheet_exists="replace") as writer:
        df_iocov.to_excel(writer, sheet_name=sheet_iocov_details, index=False)

    # Prepare Bug Report for Original
    make_bug_report(df_orig, os.path.join(cur_dir, orig_res_dir))
    with pd.ExcelWriter(report_file, mode="a", engine="openpyxl", if_sheet_exists="replace") as writer:
        df_orig.to_excel(writer, sheet_name=sheet_orig_details, index=False)



def remove_duplicate(sheet_details, sheet_unique):
    # Read the data
    df = pd.read_excel(report_file, sheet_name=sheet_details)

    # Group by the two columns and aggregate the test cases into a comma-separated list
    df_grouped = (
        df.groupby(["Bug Consequence", "Operation Sequence"])["Test Workload Name"]
        .apply(lambda x: ", ".join(sorted(set(x.dropna()))))  # Remove NaN, deduplicate, sort
        .reset_index()
        .rename(columns={"Test Workload Name": "Test Workloads"})  # Rename for clarity
    )

    # Write to another sheet
    with pd.ExcelWriter(report_file, mode='a', if_sheet_exists='replace', engine="openpyxl") as writer:
        df_grouped.to_excel(
            writer,
            sheet_name=sheet_unique,
            index=False,
            columns=["Test Workloads", "Bug Consequence", "Operation Sequence"]
        )



def main():
    generate_all_bugs()
    
    # Generate unique bugs for IOCov
    remove_duplicate(sheet_iocov_details, sheet_iocov_unique)

    # Generate unique bugs for Original
    remove_duplicate(sheet_orig_details, sheet_orig_unique)




if __name__ == "__main__":
    main()