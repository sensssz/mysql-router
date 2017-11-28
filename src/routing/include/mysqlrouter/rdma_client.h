#ifndef RDMA_RDMA_CLIENT_H_
#define RDMA_RDMA_CLIENT_H_

#include "rdma_communicator.h"

#include <cstddef>

class RdmaClient : public RdmaCommunicator {
public:
  RdmaClient(std::string hostname, int port);
  Status Connect();
  char *GetRemoteBuffer();
  ssize_t SendToServer(void *buffer, size_t size);
  void Disconnect();
  Status CancelOustanding();
  ssize_t Read(void *buffer, size_t size) {
    return context_->buffer.Read(buffer, size);
  }
  bool HasData() {
    return context_->buffer.HasData();
  }

protected:
  virtual Status OnAddressResolved(struct rdma_cm_id *id) override;
  virtual Status OnRouteResolved(struct rdma_cm_id *id) override;
  virtual Status OnConnectRequest(struct rdma_cm_id *id) override;

private:
  int port_;
  std::string hostname_;
  Context *context_;
};

#endif // RDMA_RDMA_CLIENT_H_
