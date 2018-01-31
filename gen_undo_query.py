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

def get_insert_item_bid_undo(sql):
  return get_insert_undo(sql, 'ITEM_BID', ['ib_id', 'ib_i_id', 'ib_u_id'])

def get_insert_item_max_bid_undo(sql):
  return get_insert_undo(sql, 'ITEM_MAX_BID', ['imb_i_id', 'imb_u_id'])

def get_insert_item_undo(sql):
  return get_insert_undo(sql, 'ITEM', ['i_id', 'i_u_id'])

def get_insert_item_attribute_und0(sql):
  return get_insert_undo(sql, 'ITEM_ATTRIBUTE', ['ia_id', 'ia_i_id', 'ia_u_id'])

def get_insert_item_image_und0(sql):
  return get_insert_undo(sql, 'ITEM_IMAGE', ['ii_id', 'ii_i_id', 'ii_u_id'])

def get_insert_item_purchase_undo(sql):
  return get_insert_undo(sql, 'ITEM_PURCHASE', ['ip_id', 'ip_ib_id', 'ip_ib_i_id', 'ip_ib_u_id'])

def main():
  sql_file = open('/home/jiamin/auctionmark.sql')
  undo_file = open('/home/jiamin/auctionmark.undo', 'w')
  i = 0
  table_handlers = {
      'ITEM_BID': get_insert_item_bid_undo,
      'ITEM_MAX_BID': get_insert_item_max_bid_undo,
      'ITEM': get_insert_item_undo,
      'ITEM_ATTRIBUTE': get_insert_item_attribute_und0,
      'ITEM_IMAGE': get_insert_item_image_und0,
      'ITEM_PURCHASE': get_insert_item_purchase_undo
  }
  for line in sql_file:
    undo = ''
    for table, handler in table_handlers.iteritems():
      if line.startswith('INSERT INTO ' + table):
        undo = handler(line)
        break
    if len(undo) > 0:
      undo_file.write(str(i) + ' ' + undo + '\n')
    i += 1
  undo_file.close()


if __name__ == '__main__':
  main()
