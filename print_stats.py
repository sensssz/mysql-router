import sys
import numpy as np

def load_latencies(filename):
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

def calc_stats(ori_latencies, sqp_latencies, query_count):
  for query_id in ori_latencies:
    ori_query_latencies = ori_latencies[query_id]
    sqp_query_latencies = sqp_latencies[query_id]
    ori_avg = np.mean(ori_query_latencies)
    sqp_avg = np.mean(sqp_query_latencies)
    std_to_avg = np.std(ori_query_latencies) / ori_avg
    speedup = ori_avg / sqp_avg
    propotion = len(ori_query_latencies) * 100 / query_count
    print '%d, %f, %f, %f, %f, %f' % (query_id, propotion, sqp_avg, ori_avg, std_to_avg, speedup)

def main(original_latency_file, sqp_latency_file):
  ori_latencies, query_count = load_latencies(original_latency_file)
  sqp_latencies, query_count = load_latencies(sqp_latency_file)
  calc_stats(ori_latencies, sqp_latencies, query_count)

if __name__ == '__main__':
  if len(sys.argv) != 3:
    print 'Usage %s [original_latency_file] [sqp_latency_file]' % sys.argv[0]
  main(sys.argv[1], sys.argv[2])