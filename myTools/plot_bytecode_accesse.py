import pandas as pd
import matplotlib.pyplot as plt
import sys

def read_data(file_path):
    data = {'MemoryAddress': [], 'InsAddress': []}
    with open(file_path, 'r') as file:
        for line in file:
            if 'MemoryAddress' in line:
                parts = line.strip().split()
                data['MemoryAddress'].append(int(parts[1], 0))
                data['InsAddress'].append(int(parts[3], 0))
    return pd.DataFrame(data)

def plot_addresses(df, column, title, filename):
    plt.figure(figsize=(10, 6))
    plt.scatter(range(len(df)), df[column], s=0.5)  # s is the size of the point
    plt.title(title)
    plt.xlabel('Sequential Order')
    plt.ylabel('Address')
    plt.savefig(f"analysis/{filename}")
    plt.close()

# Example usage
if __name__ == "__main__":
    file_path = sys.argv[1] # Replace with your file path
    df = read_data(file_path)

    plot_addresses(df, 'MemoryAddress', 'Memory Accesses', "MemoryAddresses")
    plot_addresses(df, 'InsAddress', 'Instruction Accesses', "InsAddresses")

