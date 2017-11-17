#ifndef RDMA_RDMA_COMMUNICATOR_H_
#define RDMA_RDMA_COMMUNICATOR_H_

#include "context.h"
#include "status.h"

#include <iostream>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <rdma/rdma_cma.h>

#define ERROR_IF_NON_ZERO(x) \
  do { \
    if ((x)) { \
      std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " #x " returned non-zero with message " << strerror(errno) << std::endl; \
      return Status::Err(); \
    } \
  } while (0)

#define RETURN_IF_NON_ZERO(x) \
  do { \
    if ((x)) { \
      std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " #x " returned non-zero with message " << strerror(errno) << std::endl; \
      return nullptr; \
    } \
  } while (0)

#define ERROR_IF_ZERO(x) \
  do { \
    if (!(x)) { \
      std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " #x " returned zero with message " << strerror(errno) << std::endl; \
      return Status::Err(); \
    } \
  } while (0)

#define RETURN_IF_ZERO(x) \
  do { \
    if (!(x)) { \
      std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] " #x " returned zero with message " << strerror(errno) << std::endl; \
      return nullptr; \
    } \
  } while (0)


class RdmaCommunicator {
public:
  RdmaCommunicator();
  virtual ~RdmaCommunicator() {}

  static Status PostReceive(Context *context);
  static Status PostSend(Context *context, size_t size);

protected:
  static void OnWorkCompletion(Context *context, struct ibv_wc *wc);
  static void *PollCompletionQueue(void *context);

  virtual Status OnAddressResolved(struct rdma_cm_id *id) = 0;
  virtual Status OnRouteResolved(struct rdma_cm_id *id) = 0;
  virtual Status OnConnectRequest(struct rdma_cm_id *id) = 0;

  virtual Status OnConnection(struct rdma_cm_id *id);
  virtual Status OnDisconnect(struct rdma_cm_id *id);
  virtual void DestroyConnection(void *context);
  virtual Status InitContext(Context *context, struct rdma_cm_id *id);
  virtual StatusOr<Context> BuildContext(struct rdma_cm_id *id);

  Status OnEvent(struct rdma_cm_event *event);

  void BuildQueuePairAttr(Context *context, struct ibv_qp_init_attr* attributes);
  void BuildParams(struct rdma_conn_param *params);
  Status RegisterMemoryRegion(Context *context);

protected:
  struct rdma_cm_id *cm_id_;
  struct rdma_event_channel *event_channel_;
};

#endif // RDMA_RDMA_COMMUNICATOR_H_
