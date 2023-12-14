import sys
import numpy as np
from collections import Counter

def read_tool_output(filename):
    with open(filename, 'r') as file:
        print(sum(1 for line in file))
        file.seek(0)
        return {line.split()[1][2:] for line in file}

def read_debug_output(filename):
    with open(filename, 'r') as file:
        print(sum(1 for line in file))
        file.seek(0)
        return {line.split()[1] for line in file}
    

def read_debug_output_array(filename):
    print("Reading debug file")
    with open(filename, 'r') as file:
        return np.array([line.split()[1] for line in file])
def read_tool_output_array(filename):
    print("Reading tool file")
    with open(filename, 'r') as file:
        return np.array([line.split()[1][2:] for line in file])
    
def extract_assembly_context(instruction_addresses, assembly_file_path, lines_above=5, lines_below=10):
    with open(assembly_file_path, 'r') as file:
        assembly_lines = file.readlines()

    results = []
    line_numbers = len(assembly_lines)
    for line_number, line in enumerate(assembly_lines):
        # Check if the line contains any of the instruction addresses
        print (f"{round((line_number/line_numbers)*100, 2)}" + " percent complete", end="\r", flush=True)
        try:
            if any(addr in line.strip().split()[0] for addr in instruction_addresses):
                # Calculate the range of lines to extract
                start = max(0, line_number - lines_above)
                end = min(len(assembly_lines), line_number + lines_below + 1)
                context = assembly_lines[start:end]

                # Add the extracted lines to the results
                results.append(''.join(context))
        except:
            continue

    return results



def process_files(tool_output_file, debug_output_file, output_file, assembly_file_path):
    debug_array = read_debug_output_array(debug_output_file)
    tool_output_array = read_tool_output_array(tool_output_file)
    ca = Counter(debug_array)
    cb = Counter(tool_output_array)

    inital_size_debug = ca.total()
    print(inital_size_debug)
    inital_size_tool = cb.total()

    result_a = sorted((ca - cb).elements())
    result_b = sorted((cb - ca).elements())
    print(f"Left of debug array: {round(len(result_a)/inital_size_debug, 4) * 100} %")
    print(f"Left of tool array: {round(len(result_b)/inital_size_tool, 4) * 100}")

    tool_output_data = read_tool_output(tool_output_file)
    debug_output_data = read_debug_output(debug_output_file)






    # Count addresses in debug output not found in tool output
    count_not_found = sum(1 for addr in debug_output_data if addr not in tool_output_data)
    print(f"Not found {count_not_found} of {len(debug_output_data)} = {(count_not_found/len(debug_output_data)) * 100}% of loads")
    wrong_count = sum(1 for addr in tool_output_data if addr not in debug_output_data)
    print(f"Wrongly found addresses {wrong_count}")
    

    # Find lines in tool output not found in debug output
    unmatched_ins = set()
    unmatched_lines = []
    # for addr, ins_addr in tool_output_data.items():
    #     if addr not in debug_output_data and ins_addr not in unmatched_ins:
    #         unmatched_lines.append(f"MemoryAddress: {addr} InsAddress: {ins_addr}\n")
    #         unmatched_ins.add(ins_addr)

    # Write results to output file
    with open(output_file, 'w') as file:
        print(f"Number of Addresses in 'debugOutput' not found in 'toolOutput': {count_not_found}\n")
        file.write(f"Number of Addresses in 'debugOutput' not found in 'toolOutput': {count_not_found}\n")
        file.write("Lines in 'toolOutput' not found in 'debugOutput':\n")
        file.writelines(unmatched_lines)
    if (assembly_file_path == ""): 
        return
    instruction_addresses = [ins_addr for _, ins_addr in tool_output_data.items()]
    instruction_addresses = set(instruction_addresses)
    extracted_contexts = extract_assembly_context(instruction_addresses, assembly_file_path)
    with open("analysis/assembly_context.txt", "w") as file:
        for line in extracted_contexts:
            file.write(line + "\n")
    

# Example usage
if __name__ == "__main__":
    if (len(sys.argv) == 5):
        process_files(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        process_files(sys.argv[1], sys.argv[2], sys.argv[3], "")
