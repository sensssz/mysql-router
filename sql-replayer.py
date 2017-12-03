#!/usr/bin/env python

import sys
import time

import mysql.connector

from mysql.connector import errorcode

def load_work_trace(filename):
  ''' Load the workload trace and return all SQL queries.
  '''
  print 'Loading workload trace...'
  with open(filename, 'r') as infile:
    sqls = [x for x in infile if len(x) > 1]
  print 'Workload trace loaded'
  return sqls

def connect_to_db():
  ''' Establish database connection
  '''
  print 'Connecting to database...'
  try:
    connection = mysql.connector.connect(user='root', host='ln001', database='redmine')
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
  cursor = connection.cursor()
  # think time in ms
  think_time = 100
  print 'Replay starts'
  i = 1
  total = len(sqls)
  for sql in sqls:
    sys.stdout.write('\rReplay of %d/%d' % (i, total))
    if sql == 'START':
      continue
    elif sql == 'COMMIT':
      connection.commit()
    else:
      cursor.execute(sql)
    time.sleep(think_time * 1e-6)
  print '\n Replay finished'

def main():
  ''' Main function
  '''
  if len(sys.argv) != 2:
    print 'Usage: ./sql-replayer.py [workload_trace]'
    sys.exit(1)
  connection = connect_to_db()
  sqls = load_work_trace(sys.argv[1])
  replay(connection, sqls)

if __name__ == '__main__':
  main()
