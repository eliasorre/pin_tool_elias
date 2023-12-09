import matplotlib.pyplot as plt
import sys
import os


def plot_data_togheter(direct, indirect, title, filename):
    direct_timstamps, direct_addresses = zip(*direct)
    indirect_timstamps, indirect_addresses = zip(*indirect)

    # Plotting
    plt.figure()
    plt.scatter(direct_timstamps, direct_addresses, c="r", s=0.01)
    plt.scatter(indirect_timstamps, indirect_addresses, c="b", s=0.01)
    plt.xlabel("TimeStamp")
    plt.ylabel("Address")
    plt.title(title)
    plt.tight_layout()
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.show()
    plt.savefig(os.path.join("analysis", filename))
    plt.close()

def plot_data_single(data, title, filename):
    timstamps, addresses = zip(*data)

    # Plotting
    plt.figure(figsize=(15, 7))
    plt.scatter(timstamps, addresses, c="r", s=0.01)
    plt.xlabel("TimeStamp")
    plt.ylabel("Address")
    plt.title(title)
    plt.tight_layout()
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.savefig(os.path.join("analysis", filename))
    plt.close()

def main(filename):
    with open(f"/home/elias/SemesterProject/pin_tool_elias/memoryReadsR13.out", 'r') as f:
        lines = f.readlines()

    direct_jumps = []
    indirect_jumps = []

    i = 0
    for line in lines:
        if (number_of_jmps < i and number_of_jmps > 0): 
            break
        i = i + 1
        try:
            address, direct, timestamp, last_read = line.split()
            if int(last_read) == 1:
                direct_jumps.append((int(timestamp), int(address, 0)))
            else:
                indirect_jumps.append((int(timestamp), int(address, 0)))
            # if int(address, 0) < 1*10**14 or int(address, 0) > 1.402 * 10**14:
            #     continue 
            #if int(direct) == 1:
             #   direct_jumps.append((int(timestamp), int(address, 0)))

        except:
            continue
    
    # Plot memory data
    # plot_data_togheter(direct_jumps, indirect_jumps, "Memory Load Addresses", "r13_memory_loads.png")
    plot_data_single(direct_jumps, "Memory Load Addresses before JMP", "r13_memory_loads_before.png")
    plot_data_single(indirect_jumps, "Memory Load Addresses not before JMP", "r13_memory_loads_n_before.png")

if __name__ == "__main__":
    if (len(sys.argv) > 1): 
        filename = sys.argv[1]
        number_of_jmps = int(sys.argv[2])
        print(filename)
        main(filename)
    else: 
        number_of_jmps = 1000
        main("memoryReadsR13.out")




