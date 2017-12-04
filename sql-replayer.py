#!/usr/bin/env python

import json
import sys
import time

import numpy as np
import mysql.connector

from mysql.connector import errorcode

def parse_query(line):
  ''' Parse the line and return a SQL query.
  '''
  json_obj = json.loads(line)
  return json_obj['sql'], json_obj['time']

def load_work_trace(filename):
  ''' Load the workload trace and return all SQL queries.
  '''
  sqls = []
  prev_timestamp = 0
  print 'Loading workload trace...'
  with open(filename, 'r') as infile:
    for line in infile:
      if len(line) < 2:
        continue
      sql, timestamp = parse_query(line)
      if prev_timestamp == 0:
        think_time = 0
      else:
        think_time = timestamp - prev_timestamp
      sqls.append((sql, think_time))
      prev_timestamp = timestamp
  print 'Workload trace loaded'
  return sqls

def connect_to_db(server):
  ''' Establish database connection
  '''
  print 'Connecting to database...'
  try:
    connection = mysql.connector.connect(user='root', host=server, port=4243, database='lobsters')
    connection.autocommit = False
  except mysql.connector.Error as err:
    if err.errno == errorcode.ER_ACCESS_DENIED_ERROR:
      print "Something is wrong with your user name or password"
    elif err.errno == errorcode.ER_BAD_DB_ERROR:
      print "Database does not exist"
    else:
      print err
  print 'Connection established.'
  return connection

def replay(connection, sqls):
  ''' Replay the SQL queries.
  '''
  cursor = connection.cursor(buffered=True)
  print 'Replay starts'
  i = 0
  total = len(sqls)
  latencies = []
  try:
    for sql, think_time in sqls:
      i += 1
      sys.stdout.write('\rReplay of %d/%d' % (i, total))
      if sql == 'START':
        trx_start = time.time()
        continue
      if think_time > 0:
        time.sleep(think_time * 1e-6)
      if sql == 'COMMIT':
        connection.commit()
        duration = time.time() - trx_start
        latencies.append(duration.total_seconds() * 1e6)
      else:
        cursor.execute(sql)
        if cursor.rowcount > 0:
          cursor.fetchall()
  except mysql.connector.Error as err:
    print err
  print '\n Replay finished'
  return latencies

def dump_latencies(latencies, filename):
  ''' Dump the latency data
  '''
  latency_file = open(filename, 'w')
  for latency in latencies:
    latency_file.write('%s\n', latency)
  latency_file.close()
  print 'Mean latency is %fus.' % np.mean(latencies)

def main():
  ''' Main function
  '''
  if len(sys.argv) != 4:
    print 'Usage: ./sql-replayer.py [server] [workload_trace] [latency_file]'
    sys.exit(1)
  connection = connect_to_db(sys.argv[1])
  sqls = load_work_trace(sys.argv[2])
  latencies = replay(connection, sqls)
  dump_latencies(latencies, sys.argv[3])

if __name__ == '__main__':
  main()
