#!/usr/bin/env python

import sys
import numpy as np

def load_latencies(filename):
  ''' Load latencies and query ids from file.
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


def load_plain_latencies(filename):
  ''' Load latencies from file.
  '''
  infile = open(filename, 'r')
  latencies = []
  for line in infile:
    latency = int(line)
    latencies.append(latency)
  return latencies


def calc_stats(ori_latencies, sqp_latencies, query_count, aggregate_name, aggregator):
  ''' Calculate stats about original latency and SQP latency
  '''
  ori_latencies_all = []
  sqp_latencies_all = []
  for query_id in ori_latencies:
    ori_query_latencies = ori_latencies[query_id]
    sqp_query_latencies = sqp_latencies[query_id]
    ori_latencies_all += ori_query_latencies
    sqp_latencies_all += sqp_query_latencies
  ori_latency_stat = aggregator(ori_latencies_all)
  sqp_latency_stat = aggregator(sqp_latencies_all)
  overall_speedup = ori_latency_stat / sqp_latency_stat
  print '%s, Overall, 100, %f, %f, %f\n' % (aggregate_name, ori_latency_stat,
                                            sqp_latency_stat, overall_speedup)


def calc_stats_for_query_types(tag):
  ''' Calculate stats about each query type
  '''
  ori_latencies, query_count = load_latencies('latencies/query_process_' + tag + '_ori')
  sqp_latencies, query_count = load_latencies('latencies/query_process_' + tag + '_sqp')
  print 'Stat, Query Type, Proportion (%), Original (us), SQP (us), Speedup of Speculation (x)'
  calc_stats(ori_latencies, sqp_latencies, query_count, 'Average', np.mean)
  calc_stats(ori_latencies, sqp_latencies, query_count, 'Median', np.median)
  calc_stats(ori_latencies, sqp_latencies, query_count,
             '95th Percentile', lambda x: np.percentile(x, 95))
  calc_stats(ori_latencies, sqp_latencies, query_count,
             '99th Percentile', lambda x: np.percentile(x, 99))


def calc_plain_stats(query_type, ori_latencies, sqp_latencies, aggregate_name, aggregator):
  ''' Calculate stats about original latency and SQP latency
  '''
  ori_latency_stat = aggregator(ori_latencies)
  sqp_latency_stat = aggregator(sqp_latencies)
  overall_speedup = ori_latency_stat / sqp_latency_stat
  print '%s, %s, %f, %f, %f' % (query_type, aggregate_name, ori_latency_stat,
                                sqp_latency_stat, overall_speedup)


def calc_stats_for_type(query_type, tag):
  ''' Calculate stats about one query type
  '''
  ori_latencies = load_plain_latencies('latencies/' + query_type + '_latencies_' + tag + '_ori')
  sqp_latencies = load_plain_latencies('latencies/' + query_type + '_latencies_' + tag + '_sqp')
  calc_plain_stats(query_type, ori_latencies, sqp_latencies, 'Average', np.mean)
  calc_plain_stats(query_type, ori_latencies, sqp_latencies, 'Median', np.median)
  calc_plain_stats(query_type, ori_latencies, sqp_latencies, '95th Percentile', lambda x: np.percentile(x, 95))
  calc_plain_stats(query_type, ori_latencies, sqp_latencies, '99th Percentile', lambda x: np.percentile(x, 99))


def calc_stats_for_read_write(tag):
  ''' Calculate stats about reads and writes
  '''
  print 'Query Type, Stat, Original (us), SQP (us), Speedup of Speculation (x)'
  calc_stats_for_type('read', tag)
  calc_stats_for_type('write', tag)


def main(tag):
  ''' Main function
  '''
  calc_stats_for_query_types(tag)
  calc_stats_for_read_write(tag)

if __name__ == '__main__':
  if len(sys.argv) != 2:
    print 'Usage %s [tag]' % sys.argv[0]
  main(sys.argv[1])
