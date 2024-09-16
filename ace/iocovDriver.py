import random

############### Constant Variables ###############
BYTES_200_MiB = 209715200
BYTE_POWER_MAX = 64

############### Original CrashMonkey Arguments ###############
### opendir
DEFAULT_OPENDIR_MODE = ' 0777'
### open
DEFAULT_OPEN_FLAGS = ' O_RDWR|O_CREAT'
DEFAULT_OPEN_MODE = ' 0777'
### mkdir
DEFAULT_MKDIR_MODE = ' 0777'

############### IOCov-Improved CrashMonkey Arguments ###############
### opendir
########## opendir mode (open a writable dir)
"""
'0777': [CrashMonkey default], Full access for the owner, group, and others (read, write, and execute permissions for all).
'0700': Full access for the owner (read, write, and execute), no access for group or others.
'0744': Full access for the owner, read-only for group and others.
'0766': Full access for the owner, read and write for group and others.
'0770': Full access for the owner and group, no access for others.
"""
IOCOV_OPENDIR_MODE_LIST = [
    '0777', 
    '0700',
    '0744',
    '0766',
    '0770'
]
### open
########## open flags (create a writable file)
"""
O_CREAT | O_RDWR: [CrashMonkey default], Creates a file if it doesn't exist, read-write mode
O_CREAT | O_WRONLY: Creates a file if it doesn't exist, write-only mode
O_CREAT | O_WRONLY | O_TRUNC: Creates a file if it doesn't exist, truncates the file if it exists, write-only mode
O_CREAT | O_RDWR | O_TRUNC: Creates a file if it doesn't exist, truncates the file if it exists, read-write mode
O_CREAT | O_WRONLY | O_APPEND: Creates a file if it doesn't exist, write-only mode, appends data to the end of the file
O_CREAT | O_RDWR | O_APPEND: Creates a file if it doesn't exist, read-write mode, appends data to the end of the file
Note: no O_EXCL should be added, as it will cause the open to fail if the file already exists.
"""
IOCOV_OPEN_FLAGS_LIST = [
    'O_CREAT|O_RDWR',
    'O_CREAT|O_WRONLY',
    'O_CREAT|O_WRONLY|O_TRUNC',
    'O_CREAT|O_RDWR|O_TRUNC',
    'O_CREAT|O_WRONLY|O_APPEND',
    'O_CREAT|O_RDWR|O_APPEND'
]
########## open mode (create a writable file)
"""
'0777': [CrashMonkey default], Full permissions for owner, group, and others.
'0700': Full permissions for the owner, no permissions for group or others (owner can write).
'0744': Owner has full permissions, group and others have read-only permissions (owner can write).
'0666': Read and write permissions for owner, group, and others.
'0644': Owner has read and write permissions, group and others have read-only permissions.
'0600': Owner has read and write permissions, no permissions for group or others.
'0766': Owner has full permissions, group and others have read and write permissions.
"""
IOCOV_OPEN_MODE_LIST = [
    '0777',
    '0700',
    '0744',
    '0666',
    '0644',
    '0600',
    '0766'
]
### mkdir
########## mkdir mode (create a writable dir)
"""
'0777': [CrashMonkey default], Full permissions for the owner, group, and others (read, write, and execute for all).
'0700': Full permissions for the owner (read, write, and execute), no permissions for group or others.
'0744': Full permissions for the owner, read-only permissions for group and others.
'0766': Full permissions for the owner, read and write permissions for group and others.
'0770': Full permissions for the owner and group, no permissions for others.
"""
IOCOV_MKDIR_MODE_LIST = [
    '0777', 
    '0700', 
    '0744', 
    '0766', 
    '0770'
]

### TODO: mknod

# Utility function to generate a list of powers of 2, with +1, -1 offsets
# 200 MiB = 209,715,200 bytes.
def generate_powers_of_2_with_limits(dev_bytes):
    result_set = set()  # Use a set to avoid duplicates

    # Loop through powers of 2, from 2^0 to 2^64
    for i in range(BYTE_POWER_MAX + 1):  # Include 2^64, so we go up to 65
        power_of_2 = 2 ** i

        # Add power_of_2 within dev_bytes
        if power_of_2 <= dev_bytes:
            result_set.add(power_of_2)    
           
        # Add power_of_2 + 1, and power_of_2 - 1, if within dev_bytes
        if power_of_2 + 1 <= dev_bytes:
            result_set.add(power_of_2 + 1)
        
        if power_of_2 - 1 > 0 and power_of_2 - 1 <= dev_bytes:
            result_set.add(power_of_2 - 1)

    # Convert the set to a sorted list and return it
    return sorted(result_set)

def generate_byte_offsets_only_with_limits(dev_bytes):
    result_set = set()  # Use a set to avoid duplicates

    # Loop through powers of 2, from 2^0 to 2^64
    for i in range(BYTE_POWER_MAX + 1):  # Include 2^64, so we go up to 65
        power_of_2 = 2 ** i

        # Add power_of_2 + 1, and power_of_2 - 1, if within dev_bytes
        
        if power_of_2 + 1 <= dev_bytes:
            result_set.add(power_of_2 + 1)
        
        if power_of_2 - 1 > 0 and power_of_2 - 1 <= dev_bytes:
            result_set.add(power_of_2 - 1)

    # Convert the set to a sorted list and return it
    return sorted(result_set)

### falloc

###### falloc append mode: off is the file size, no need to set

########## falloc lenn
IOCOV_FALLOC_APPEND_LENN_LIST = generate_powers_of_2_with_limits(BYTES_200_MiB)

###### falloc overlap_unaligned_start: off should be 0 due to start, no need to set
IOCOV_FALLOC_OUS_LENN_LIST = generate_byte_offsets_only_with_limits(BYTES_200_MiB)


############### IOCov-Improved CrashMonkey Argument Selection ###############

def create_random_selector(arglist):
    def select_random_element():
        return random.choice(arglist)
    return select_random_element

# Create random selector functions for each syscall argument list
opendir_mode_rs = create_random_selector(IOCOV_OPENDIR_MODE_LIST)
open_flags_rs = create_random_selector(IOCOV_OPEN_FLAGS_LIST)
open_mode_rs = create_random_selector(IOCOV_OPEN_MODE_LIST)
creat_flags_rs = create_random_selector(IOCOV_OPEN_FLAGS_LIST)
creat_mode_rs = create_random_selector(IOCOV_OPEN_MODE_LIST)
mkdir_mode_rs = create_random_selector(IOCOV_MKDIR_MODE_LIST)




