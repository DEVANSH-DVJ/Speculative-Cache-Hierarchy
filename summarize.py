import os
import sys

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def extract(result_file: str):
    print(result_file)
    params = result_file.split('/')
    num_instructions = params[0][8:]
    model = params[1]
    trace_type = params[2]
    trace_name = params[3]

    with open(result_file, 'r') as f:
        lines = f.readlines()
        l21 = lines[21].split()
        IPC = l21[4]
        cycles = l21[8]

    return num_instructions, model, trace_type, trace_name, IPC, cycles


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python run.py <results_list>')
        sys.exit(1)

    filename = sys.argv[1]

    results = []
    with open(os.path.join(BASE_DIR, filename)) as f:
        for line in f:
            results.append(line.strip())

    print(results)
    print('Total results: {}'.format(len(results)))
    print(extract(results[0]))
