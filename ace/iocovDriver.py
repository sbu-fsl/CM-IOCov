import random

############### Constant Variables ###############

# Example lower and upper limits for the number of bytes
LOWER_MIN = 16
UPPER_BYTES_200MIB = 209715200
# The byte size for a file should be FILE_DEV_RATE * dev_bytes where FILE_DEV_RATE is (0, 1)
FILE_DEV_RATE = 0.01

# Set the lower and upper limits based on certain constants
LOWER_TO_USE = LOWER_MIN
UPPER_TO_USE = UPPER_BYTES_200MIB * FILE_DEV_RATE

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

# TODO: 09/16/2024 investigate other flags: 0755, 0701, 0500, etc.

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

# Utility functions to generate a list of powers of 2, with +1, -1 offsets
# lower_limit and upper_limit are inclusive
# 200 MiB = 209,715,200 bytes.
def gen_powers_two_offsets_limits(lower_limit, upper_limit):
    result_set = set()  # Use a set to avoid duplicates
    # Loop through powers of 2, from 2^0 to 2^64
    for i in range(BYTE_POWER_MAX + 1):  # Include 2^64, so we go up to 65
        power_of_2 = 2 ** i

        # Add power_of_2 within dev_bytes
        if power_of_2 <= upper_limit and power_of_2 >= lower_limit:
            result_set.add(power_of_2)  
           
        # Add power_of_2 + 1, and power_of_2 - 1, if within the upper/lower limits
        if power_of_2 + 1 <= upper_limit and power_of_2 + 1 >= lower_limit:
            result_set.add(power_of_2 + 1)
        
        # Add power_of_2 - 1, if within the upper/lower limits
        if power_of_2 - 1 >= lower_limit and power_of_2 - 1 <= upper_limit:
            result_set.add(power_of_2 - 1)
    # Avoid returning an empty list if no elements are added
    if len(result_set) == 0:
        return [lower_limit, upper_limit]
    # Convert the set to a sorted list and return it
    return sorted(result_set)

# Utility function to generate a list of powers of 2, with no offsets
def gen_only_powers_two_limits(lower_limit, upper_limit):
    result_set = set()  # Use a set to avoid duplicates
    # Loop through powers of 2, from 2^0 to 2^64
    for i in range(BYTE_POWER_MAX + 1):  # Include 2^64, so we go up to 65
        power_of_2 = 2 ** i

        # Add power_of_2 within dev_bytes
        if power_of_2 <= upper_limit and power_of_2 >= lower_limit:
            result_set.add(power_of_2)
    # Convert the set to a sorted list and return it
    return sorted(result_set)

# Utility function to generate a list of byte offsets, with no powers of 2
def gen_only_byte_offsets_limits(lower_limit, upper_limit):
    result_set = set()  # Use a set to avoid duplicates
    # Loop through powers of 2, from 2^0 to 2^64
    for i in range(BYTE_POWER_MAX + 1):  # Include 2^64, so we go up to 65
        power_of_2 = 2 ** i

        # Add power_of_2 + 1, and power_of_2 - 1, if within dev_bytes
        if power_of_2 + 1 <= upper_limit and power_of_2 + 1 >= lower_limit:
            result_set.add(power_of_2 + 1)

        if power_of_2 - 1 >= lower_limit and power_of_2 - 1 <= upper_limit:
            result_set.add(power_of_2 - 1)
    # Convert the set to a sorted list and return it
    return sorted(result_set)

### falloc/write

###### falloc/write "append" mode: off is the file size, no need to set
IOCOV_FALLOC_APPEND_LENN_LIST = gen_powers_two_offsets_limits(LOWER_TO_USE, UPPER_TO_USE)

###### falloc/write "overlap_unaligned_start" mode: off should be 0 due to start, no need to set
IOCOV_FALLOC_OUS_LENN_LIST = gen_only_byte_offsets_limits(LOWER_TO_USE, UPPER_TO_USE)

###### falloc/write "overlap_unaligned_end" mode
IOCOV_FALLOC_OUE_LENN_LIST = gen_only_byte_offsets_limits(LOWER_TO_USE, UPPER_TO_USE)

###### falloc/write "overlap_extend" mode
IOCOV_FALLOC_OVEREXT_LENN_LIST = gen_powers_two_offsets_limits(LOWER_TO_USE, UPPER_TO_USE)

###### truncate "aligned" mode
IOCOV_TRUNCATE_ALIGHED_LEN_LIST = gen_only_powers_two_limits(LOWER_TO_USE, UPPER_TO_USE)

###### truncate "unaligned" mode
IOCOV_TRUNCATE_UNALIGHED_LEN_LIST = gen_only_byte_offsets_limits(LOWER_TO_USE, UPPER_TO_USE)

############### IOCov-Improved CrashMonkey Argument Selection ###############

def create_random_selector(arglist):
    def select_random_element():
        return random.choice(arglist)
    return select_random_element

# Return a random integer (e.g., offset) smaller than the given size
def get_random_smaller_int(size):
    if size <= 1:
        return 1
    
    return random.randint(1, size - 1)

# Special case: return a random number with special lower and upper limits
def customized_rs_limits(gen_func, lower_limit, upper_limit):
    rs_func =  create_random_selector(gen_func(lower_limit, upper_limit))
    return rs_func()

# Create random selector functions for each syscall argument list
### flags and mode
opendir_mode_rs = create_random_selector(IOCOV_OPENDIR_MODE_LIST)
open_flags_rs = create_random_selector(IOCOV_OPEN_FLAGS_LIST)
open_mode_rs = create_random_selector(IOCOV_OPEN_MODE_LIST)
creat_flags_rs = create_random_selector(IOCOV_OPEN_FLAGS_LIST)
creat_mode_rs = create_random_selector(IOCOV_OPEN_MODE_LIST)
mkdir_mode_rs = create_random_selector(IOCOV_MKDIR_MODE_LIST)
### numeric bytes and offsets
falloc_append_lenn_rs = create_random_selector(IOCOV_FALLOC_APPEND_LENN_LIST)
falloc_ous_lenn_rs = create_random_selector(IOCOV_FALLOC_OUS_LENN_LIST)
falloc_oue_lenn_rs = create_random_selector(IOCOV_FALLOC_OUE_LENN_LIST)
falloc_overext_lenn_rs = create_random_selector(IOCOV_FALLOC_OVEREXT_LENN_LIST)
truncate_aligned_len_rs = create_random_selector(IOCOV_TRUNCATE_ALIGHED_LEN_LIST)
truncate_unaligned_len_rs = create_random_selector(IOCOV_TRUNCATE_UNALIGHED_LEN_LIST)
