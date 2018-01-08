#!/usr/bin/env python

import sys
import numpy as np

def load_latencies(filename):
  ''' Load latencies from file.
  '''
  infile = open(filename, 'r')
  latencies = {}
  query_count = 0
  for line in infile:
    query_count += 1
    parts = line.split(',')
    query_id = int(parts[0])
    latency = int(parts[1])
    if query_id not in latencies:
      latencies[query_id] = []
    latencies[query_id].append(latency)
  return latencies, query_count

def calc_stats(ori_latencies, sqp_latencies, query_count, aggregate_name, filename, aggregator):
  ''' Calculate stats about original latency and SQP latency
  '''
  ori_latencies_all = []
  sqp_latencies_all = []
  outfile = open(filename, 'w')
  for query_id in ori_latencies:
    ori_query_latencies = ori_latencies[query_id]
    sqp_query_latencies = sqp_latencies[query_id]
    ori_latencies_all += ori_query_latencies
    sqp_latencies_all += sqp_query_latencies
    ori_stat = aggregator(ori_query_latencies)
    sqp_stat = aggregator(sqp_query_latencies)
    speedup = ori_stat / sqp_stat
    propotion = 100.0 * len(ori_query_latencies) / query_count
    outfile.write('%s, %d, %f, %f, %f, %f\n' % (aggregate_name, query_id, propotion,
                                                sqp_stat, ori_stat, speedup))
  ori_latency_stat = aggregator(ori_latencies_all)
  sqp_latency_stat = aggregator(sqp_latencies_all)
  overall_speedup = ori_latency_stat / sqp_latency_stat
  outfile.write('%s, Overall, 100, %f, %f, %f\n' % (aggregate_name, ori_latency_stat,
                                                    sqp_latency_stat, overall_speedup))
  outfile.close()

def main(original_latency_file, sqp_latency_file):
  ''' Main function
  '''
  ori_latencies, query_count = load_latencies(original_latency_file)
  sqp_latencies, query_count = load_latencies(sqp_latency_file)
  calc_stats(ori_latencies, sqp_latencies, query_count, 'Average', 'average.csv', np.mean)
  calc_stats(ori_latencies, sqp_latencies, query_count, 'Median', 'median.csv', np.median)
  calc_stats(ori_latencies, sqp_latencies, query_count,
             '95th Percentile', '95perctl.csv', lambda x: np.percentile(x, 95))
  calc_stats(ori_latencies, sqp_latencies, query_count,
             '99th Percentile', '99perctl.csv', lambda x: np.percentile(x, 99))

if __name__ == '__main__':
  if len(sys.argv) != 3:
    print 'Usage %s [original_latency_file] [sqp_latency_file]' % sys.argv[0]
  main(sys.argv[1], sys.argv[2])
