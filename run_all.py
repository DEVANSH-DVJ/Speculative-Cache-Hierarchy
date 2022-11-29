import os
import sys
from multiprocessing import Pool

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

models = [
    'baseline',
    'ALL-0.0625x',
    'ALL-0.125x',
    'ALL-0.25x',
    'ALL-0.5x',
    'ALL-1x',
    'INSTR-ALL-1x',
    'INSTR-NOALL',
    'INSTR-NOL2C',
    'INSTR-NOL2C-NOLLC',
    'INSTR-NOLLC',
    'NOALL',
    'NOL2C',
    'NOL2C-NOLLC',
    'NOLLC',
    'NTH-ALL-1x',
    'NTH-NOALL',
    'NTH-NOL2C',
    'NTH-NOL2C-NOLLC',
    'NTH-NOLLC'
]


def build(model: str):
    params = model[:-4].replace('-', ' ')
    print(f'Building {model}')
    os.system(f'cd {BASE_DIR} && ./build_champsim.sh {params}')


def run(pair):
    trace: str = pair[0]
    model: str = pair[1]
    print(trace, model)
    os.system(f'cd {BASE_DIR} && ./run_champsim.sh {model} 1 1 {trace}')


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python run.py <trace_list>')
        sys.exit(1)

    filename = sys.argv[1]

    # for model in models:
    #     build(model)

    run_pairs = []
    with open(os.path.join(BASE_DIR, filename)) as f:
        for line in f:
            trace = line.strip()
            for model in models:
                run_pairs.append((trace, model))

    pool = Pool(processes=8)
    pool.map(run, run_pairs)
