#!/usr/bin/env python

import sys
import numpy as np


def load_latencies(filename):
  ''' Load latencies and query ids from file.
  '''
  infile = open(filename, 'r')
  latencies = {}
  for line in infile:
    parts = line.split(',')
    query_id = int(parts[0])
    latency = int(parts[1])
    if query_id not in latencies:
      latencies[query_id] = []
    latencies[query_id].append(latency)
  return latencies


def per_query_speedup(ori_latencies, sqp_latencies):
  ''' Calculate the average speedup of per-query speedups
  '''
  speedups = []
  for ori_latency, sqp_latency in zip(ori_latencies, sqp_latencies):
    speedups.append(float(ori_latency) / sqp_latency)
  return np.mean(speedups)


def per_query_group_speedup(ori_latencies, sqp_latencies, aggregator):
  ''' Calculate the weighted speedup of each query type with the give aggregator
  '''
  total_num_queries = 0
  speedup = 0
  for query_id in ori_latencies:
    ori_aggregate = aggregator(ori_latencies[query_id])
    sqp_aggregate = aggregator(sqp_latencies[query_id])
    num_queries = len(ori_latencies[query_id])
    speedup += num_queries * ori_aggregate / sqp_aggregate
  return speedup / total_num_queries


def all_query_speedup(ori_latencies, sqp_latencies, aggregator):
  ''' Calculate the overall speedup with the given aggregator
  '''
  all_ori_latencies = []
  all_sqp_latencies = []
  for query_id in ori_latencies:
    all_ori_latencies += ori_latencies[query_id]
    all_sqp_latencies += sqp_latencies[query_id]
  return aggregator(all_ori_latencies) / aggregator(all_sqp_latencies)


def calc_stats(ori_latencies, sqp_latencies, query_type):
  ''' Calculate stats about original latency and SQP latency
  '''
  print 'Stat, Speedup Type, Query Type, Speedup of Speculation (x)'
  print 'Average, Per Query Speedup, %s, %f\n' %\
        (query_type, per_query_speedup(ori_latencies, sqp_latencies))
  print 'Average, Per Query Group Speedup, %s, %f\n' %\
        (query_type, per_query_group_speedup(
            ori_latencies, sqp_latencies, np.mean))
  print 'Average, All Query Speedup, %s, %f\n' %\
        (query_type, all_query_speedup(ori_latencies, sqp_latencies, np.mean))

  print 'Median, Per Query Group Speedup, %s, %f\n' %\
        (query_type, per_query_group_speedup(ori_latencies, sqp_latencies,
                                             np.median))
  print 'Median, All Query Speedup, %s, %f\n' %\
        (query_type, all_query_speedup(ori_latencies, sqp_latencies,
                                       np.median))

  print '95th Percentile, Per Query Group Speedup, %s, %f\n' %\
        (query_type, per_query_group_speedup(ori_latencies, sqp_latencies,
                                             lambda x: np.percentile(x, 95)))
  print '95th Percentile, All Query Speedup, %s, %f\n' %\
        (query_type, all_query_speedup(ori_latencies, sqp_latencies,
                                       lambda x: np.percentile(x, 95)))

  print '99th Percentile, Per Query Group Speedup, %s, %f\n' %\
        (query_type, per_query_group_speedup(ori_latencies, sqp_latencies,
                                             lambda x: np.percentile(x, 99)))
  print '99th Percentile, All Query Speedup, %s, %f\n' %\
        (query_type, all_query_speedup(ori_latencies, sqp_latencies,
                                       lambda x: np.percentile(x, 99)))


def calc_stats_for_query_types(latency_type, query_type, tag):
  ''' Calculate stats about each query type
  '''
  ori_latencies = load_latencies(
      'latencies/' + latency_type + '_latencies_' + tag + '_ori')
  sqp_latencies = load_latencies(
      'latencies/' + latency_type + '_latencies_' + tag + '_sqp')
  calc_stats(ori_latencies, sqp_latencies, query_type)
  calc_stats(ori_latencies, sqp_latencies, query_type)
  calc_stats(ori_latencies, sqp_latencies, query_type)
  calc_stats(ori_latencies, sqp_latencies, query_type)


def main(tag):
    ''' Main function
    '''
    calc_stats_for_query_types('e2e_query', 'Mixed', tag)
    calc_stats_for_query_types('server_query', 'Mixed', tag)
    calc_stats_for_query_types('read', 'Read', tag)
    calc_stats_for_query_types('write', 'Write', tag)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print 'Usage %s [tag]' % sys.argv[0]
    main(sys.argv[1])
