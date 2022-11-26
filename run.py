import os
from multiprocessing import Pool

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

models = ['bimodal-no-no-no-no-lru-1core']


def run(trace):
    print(trace)
    for model in models:
        os.system(f'cd {BASE_DIR} && ./run_champsim.sh {model} 1 1 {trace}')


if __name__ == '__main__':
    trace_list = []

    filename = 'sampled_traces.txt'
    with open(os.path.join(BASE_DIR, filename)) as f:
        for line in f:
            trace = line.strip()
            trace_list.append(trace)

    pool = Pool(processes=8)
    pool.map(run, trace_list)
