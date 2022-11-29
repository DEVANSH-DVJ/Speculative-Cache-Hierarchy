# Speculative-Cache-Hierarchy

Extension of GhostMinion (https://dl.acm.org/doi/10.1145/3466752.3480074) for core with large size ROBs

## Testing

Add traces to `traces/` folder.

```bash
bash list_data.sh
python run_all.py all_traces.txt  # Time Consuming
bash list_data.sh
python extract.py results_1M_list.txt  # Extract results from ChampSim output
python summarize.py extracted_results.csv  # Summarize results for each trace
```

## Results

1-1.5% performance slowdown

### Inferences

1. Changing the size of Speculative Cache has no visible effect on performance

2. Removing Speculative Caches at any cache level without compromising on security has a drastic effect on performance

### Plots

![Inference 1](summary/all_spec_diff_size.png)
![Inference 2](summary/all_spec_diff_setup.png)
