from collections import defaultdict

# Step 1: Extract all the jump targets
with open('/home/elias/SemesterProject/pin_tool_elias/myTools/programs/test.s', 'r') as file:
    lines = file.readlines()

# Use a defaultdict to count occurrences
jump_targets = defaultdict(int)

for line in lines:
    # This extracts the jump target address (the <...> part)
    if "jmp" in line:
        target = line.split('<')
        if (len(target) > 1):
            target = target[0].split()[-1]
            jump_targets[target] += 1

# Step 2: Count the frequency of each target
most_common_targets =  sorted(jump_targets, key=jump_targets.get, reverse=True)

# Step 3: Check if this target has the "jmp *%rax" instruction
for common_target in most_common_targets:
    for line in lines:
        if common_target in line and "jmp    *%rax" in line:
            print(f"The most frequently jumped-to address associated with 'jmp *%rax' is: {common_target}, with {jump_targets[common_target]} references.")
            break
