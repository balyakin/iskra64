#!/usr/bin/env python3
"""Summarize raw benchmark repetitions. Standard-library-only."""
import argparse
import csv
import statistics
from collections import defaultdict
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('csv_files', nargs='+', type=Path)
parser.add_argument('--output', type=Path)
args = parser.parse_args()
records = []
for path in args.csv_files:
    groups = defaultdict(list)
    with path.open(newline='') as file:
        for row in csv.DictReader(file):
            key = (row['mode'],int(row['keys_per_batch']),row['algorithm'],int(row['seed']))
            groups[key].append(float(row['ns_per_key']))
    for (mode,keys,algorithm,seed),values in sorted(groups.items()):
        median = statistics.median(values)
        records.append(dict(run=path.stem,mode=mode,keys_per_batch=keys,
            algorithm=algorithm,seed=seed,repeats=len(values),median_ns_per_key=median,
            min_ns_per_key=min(values),max_ns_per_key=max(values),
            mad_ns_per_key=statistics.median(abs(v-median) for v in values),
            input_GB_per_s=8/median))
if not records:
    parser.error('no benchmark rows found')
if args.output:
    with args.output.open('w',newline='') as file:
        writer = csv.DictWriter(file, fieldnames=list(records[0]), lineterminator='\n')
        writer.writeheader()
        writer.writerows(records)
for row in records:
    print(f"{row['run']:25s} {row['mode']:8s} {row['keys_per_batch']:8d} "
          f"{row['algorithm']:22s} {row['median_ns_per_key']:.4f} ns/key")
