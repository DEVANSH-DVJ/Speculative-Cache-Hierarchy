import os
import sys
import csv

BASE_DIR = os.path.dirname(os.path.abspath(__file__))


def extract(result_file: str):
    print(result_file)
    params = result_file.split('/')
    num_instructions = params[0][8:]
    model = params[1]
    trace_type = params[2]
    trace_name = params[3][:-4]

    with open(result_file, 'r') as f:
        lines = f.readlines()
        l21 = lines[21].split()
        IPC = l21[4]

    return num_instructions, model, trace_type, trace_name, IPC


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python run.py <results_list>')
        sys.exit(1)

    filename = sys.argv[1]
    out_file = open("results.csv", 'w')

    results = []
    with open(os.path.join(BASE_DIR, filename)) as f:
        for line in f:
            results.append(line.strip())

    writer = csv.writer(out_file)
    print('Total results: {}'.format(len(results)))
    writer.writerow(['#instr', 'model', 'trace_type', 'trace_name', 'IPC'])
    for row in results:
        writer.writerow(extract(row))
    out_file.close
