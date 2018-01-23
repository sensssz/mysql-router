#ifndef RDMA_CONTEXT_H_
#define RDMA_CONTEXT_H_

#include "spsc_ring_buffer.h"

#include <atomic>
#include <thread>

#include <rdma/rdma_cma.h>

class Context {
public:
  virtual ~Context();

  struct rdma_cm_id *id;
  struct rdma_event_channel *event_channel;
  struct ibv_qp *queue_pair;
  struct ibv_context *device_context;
  struct ibv_pd *protection_domain;
  struct ibv_cq *completion_queue;
  struct ibv_comp_channel *completion_channel;

  bool connected;

  char *recv_region;
  struct ibv_mr *recv_mr;
  char *send_region;
  struct ibv_mr *send_mr;

  int queue_depth;
  std::atomic<int> unsignaled_sends;

  SpscRingBuffer buffer;

  int num_skips;

  std::thread cq_poller_thread;
};

#endif // RDMA_CONTEXT_H_
