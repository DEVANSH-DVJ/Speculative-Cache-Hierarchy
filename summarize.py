import os
import sys
import pandas as pd


BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SUMMARY_DIR = os.path.join(BASE_DIR, 'summary')

all_spec_diff_size = ['ALL-0.0625x',
                      'ALL-0.125x',
                      'ALL-0.25x',
                      'ALL-0.5x',
                      'ALL-1x']

all_spec_diff_setup = ['ALL-1x',
                       'NOALL',
                       'NOL2C',
                       'NOL2C-NOLLC',
                       'NOLLC']

nth_spec_diff_setup = ['NTH-ALL-1x',
                       'NTH-NOALL',
                       'NTH-NOL2C',
                       'NTH-NOL2C-NOLLC',
                       'NTH-NOLLC']

instr_spec_diff_setup = ['INSTR-ALL-1x',
                         'INSTR-NOALL',
                         'INSTR-NOL2C',
                         'INSTR-NOL2C-NOLLC',
                         'INSTR-NOLLC']

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print('Usage: python extract.py <results_list>')
        sys.exit(1)

    filename = sys.argv[1]
    df = pd.read_csv(os.path.join(BASE_DIR, filename))

    model_list = list(df['model'].unique())

    assert 'baseline' in model_list, 'baseline not found in model list'
    for model in all_spec_diff_size:
        assert model in model_list, 'Model {} not found'.format(model)
    for model in all_spec_diff_setup:
        assert model in model_list, 'Model {} not found'.format(model)
    for model in nth_spec_diff_setup:
        assert model in model_list, 'Model {} not found'.format(model)
    for model in instr_spec_diff_setup:
        assert model in model_list, 'Model {} not found'.format(model)

    print(df)
    trace_list = list(df['trace_name'].unique())

    res = df.pivot_table(index=['#instr', 'trace_type', 'trace_name'],
                         columns=['model'],
                         values='IPC',
                         aggfunc='mean').reset_index()

    for model in model_list:
        res[model] = res[model] / res['baseline']

    res.loc['mean'] = res.prod(axis=0) ** (1.0 / len(res))
    res.at['mean', '#instr'] = 'gmean'
    res.at['mean', 'trace_type'] = 'gmean'
    res.at['mean', 'trace_name'] = 'gmean'

    print(res)
    pd.DataFrame(res, columns=['#instr', 'trace_type', 'trace_name'] + [model for model in all_spec_diff_size]).to_csv(os.path.join(SUMMARY_DIR, 'all_spec_diff_size.csv'), index=False)
    pd.DataFrame(res, columns=['#instr', 'trace_type', 'trace_name'] + [model for model in all_spec_diff_setup]).to_csv(os.path.join(SUMMARY_DIR, 'all_spec_diff_setup.csv'), index=False)
    pd.DataFrame(res, columns=['#instr', 'trace_type', 'trace_name'] + [model for model in nth_spec_diff_setup]).to_csv(os.path.join(SUMMARY_DIR, 'nth_spec_diff_setup.csv'), index=False)
    pd.DataFrame(res, columns=['#instr', 'trace_type', 'trace_name'] + [model for model in instr_spec_diff_setup]).to_csv(os.path.join(SUMMARY_DIR, 'instr_spec_diff_setup.csv'), index=False)
