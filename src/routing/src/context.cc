#include "mysqlrouter/context.h"

Context::~Context() {
  rdma_destroy_qp(id);
  ibv_dereg_mr(recv_mr);
  ibv_dereg_mr(send_mr);
  rdma_destroy_id(id);

  cq_poller_thread.detach();

  delete recv_region;
  delete send_region;
}
