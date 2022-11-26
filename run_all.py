import os
import sys
from multiprocessing import Pool

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

models = ['bimodal-no-no-no-no-lru-1core']


def build(model: str):
    params = model[:-4].replace('-', ' ')
    print(f'Building {model}')
    os.system(f'cd {BASE_DIR} && ./build_champsim.sh {params}')


def run(trace: str):
    print(trace)
    for model in models:
        os.system(f'cd {BASE_DIR} && ./run_champsim.sh {model} 1 1 {trace}')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python run.py <trace_list>')
        sys.exit(1)

    filename = sys.argv[1]

    for model in models:
        build(model)

    traces = []
    with open(os.path.join(BASE_DIR, filename)) as f:
        for line in f:
            trace = line.strip()
            traces.append(trace)

    pool = Pool(processes=8)
    pool.map(run, traces)
