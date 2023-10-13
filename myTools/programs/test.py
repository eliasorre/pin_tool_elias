import random
import time

# Number of elements to generate
NUM_ELEMENTS = 1000000  # 1 million elements


def generate_data(num_elements):
    """Generate a list of random floating-point numbers."""
    return [random.uniform(0, 1) for _ in range(num_elements)]

if __name__ == "__main__":
    print("Generating data...")
    data = generate_data(NUM_ELEMENTS)
        
    print("Script completed!")
