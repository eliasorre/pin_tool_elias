import sys
import subprocess
from collections import defaultdict

# For debug
if (len(sys.argv) < 2):
    sys.argv = [sys.argv[0], "test.bin"]

# Step 1: Extract all the jump targets
output_bytes = subprocess.check_output('objdump -S /home/elias/SemesterProject/pin_tool_elias/myTools/programs/{}'.format(sys.argv[1]), shell=True)
lines = output_bytes.decode('utf-8').split('\n')

# Use a defaultdict to count occurrences
jump_targets = defaultdict(int)
valid_targets = set()

for line in lines:
    # This extracts the jump target address (the <...> part)
    if "jmp" in line:
        target = line.split('<')
        if (len(target) > 1):
            target = target[0].split()[-1]
            jump_targets[target] += 1
    if ("jmp    *%rax" in line):
        valid_targets.add(line.split(':')[0].strip())

# Step 2: Count the frequency of each target
most_common_targets =  sorted(jump_targets, key=jump_targets.get, reverse=True)

# Step 3: Check if this target has the "jmp *%rax" instruction
for common_target in most_common_targets:
    if (common_target in valid_targets):
        print(f"The most frequently jumped-to address associated with 'jmp *%rax' is: {common_target}, with {jump_targets[common_target]} references.")
        break
