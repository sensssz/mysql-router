#!/usr/bin/env python

def get_insert_values(sql):
  values_begin = sql.find('(') + 1 # The first one is actually not VALUES
  values_begin = sql.find('(', values_begin) + 1
  values_end = len(sql)
  return sql[values_begin:values_end].split(',')

def get_insert_undo(sql, table, pkeys):
  values = get_insert_values(sql)
  undo_query = 'DELETE FROM %s WHERE %s=%s' % (table, pkeys[0], values[0])
  for i in range(1, len(pkeys)):
    pkey = pkeys[i]
    value = values[i].strip()
    undo_query += ' AND %s=%s' % (pkey, value)
  return undo_query

def main():
  sql_file = open('/home/jiamin/auctionmark.sql')
  undo_file = open('/home/jiamin/auctionmark.undo', 'w')
  i = 0
  table_keys = {
      'ITEM': ['i_id', 'i_u_id'],
      'ITEM_ATTRIBUTE': ['ia_id', 'ia_i_id', 'ia_u_id'],
      'ITEM_BID': ['ib_id', 'ib_i_id', 'ib_u_id'],
      'ITEM_COMMENT': ['ic_id', 'ic_i_id', 'ic_u_id'],
      'ITEM_IMAGE': ['ii_id', 'ii_i_id', 'ii_u_id'],
      'ITEM_MAX_BID': ['imb_i_id', 'imb_u_id'],
      'ITEM_PURCHASE': ['ip_id', 'ip_ib_id', 'ip_ib_i_id', 'ip_ib_u_id'],
      'USERACCT_FEEDBACK': ['uf_u_id', 'uf_i_id', 'uf_i_u_id', 'uf_from_id'],
      'USERACCT_ITEM': ['ui_u_id', 'ui_i_id', 'ui_i_u_id'],
  }
  for line in sql_file:
    undo = ''
    for table, pkeys in table_keys.iteritems():
      if line.startswith('INSERT INTO ' + table):
        undo = get_insert_undo(line, table, pkeys)
        break
    if len(undo) > 0:
      undo_file.write(str(i) + ' ' + undo + '\n')
    i += 1
  undo_file.close()


if __name__ == '__main__':
  main()
