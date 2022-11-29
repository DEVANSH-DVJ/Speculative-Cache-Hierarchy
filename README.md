# Speculative-Cache-Hierarchy

Extension of GhostMinion (https://dl.acm.org/doi/10.1145/3466752.3480074) for core with large size ROBs

# Testing

Add traces to `traces/` folder.

```bash
bash list_data.sh
python run_all.py all_traces.txt # Time Consuming
bash list_data.sh
python extract.py results_1M_list.txt
python summarize.py extracted_results.csv
```
