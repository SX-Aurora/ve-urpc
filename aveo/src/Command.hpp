/**
 * @file Command.hpp
 * @brief classes for communication between main and pseudo thread
 *
 * @internal
 * @author VEO
 */
#ifndef _VEO_COMMAND_HPP_
#define _VEO_COMMAND_HPP_
#include <queue>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <urpc_common.h>
#include "ve_offload.h"
namespace veo {
class ThreadContext;

typedef enum veo_command_state CommandStatus;
typedef enum veo_queue_state QueueStatus;

/**
 * @brief base class of command handled by pseudo thread
 *
 * a command is to be implemented as a function object inheriting Command
 * (see CommandImpl in ThreadContext.cpp).
 */
class Command {
private:
  uint64_t msgid;/*! message ID */
  uint64_t retval;/*! returned value from the function on VE */
  int64_t urpc_req;/*! URPC request ID, assigned once in-flight */
  int status;

public:
  explicit Command(uint64_t id): msgid(id) {}
  Command() = delete;
  Command(const Command &) = delete;
  virtual int operator()() = 0;
  virtual int operator()(urpc_mb_t *m, void *payload, size_t plen) = 0;
  void setURPCReq(int64_t req, int s) { this->urpc_req = req; this->status = s; }
  int64_t getURPCReq() { return this->urpc_req; }
  void setResult(uint64_t r, int s) { this->retval = r; this->status = s; }
  uint64_t getID() { return this->msgid; }
  int getStatus() { return this->status; }
  uint64_t getRetval() { return this->retval; }
  bool isVH() { return false; };
};

/**
 * @brief blocking queue used in CommQueue
 */
class BlockingQueue {
private:
  std::mutex mtx;
  std::condition_variable cond;
  std::deque<std::unique_ptr<Command> > queue;
  std::unique_ptr<Command> tryFindNoLock(uint64_t);
  std::atomic<QueueStatus> queue_state;

public:
  BlockingQueue() : queue_state(VEO_QUEUE_READY) {}
  void push(std::unique_ptr<Command>);
  void push_front(std::unique_ptr<Command>);
  std::unique_ptr<Command> pop();
  std::unique_ptr<Command> tryFind(uint64_t);
  std::unique_ptr<Command> wait(uint64_t);
  std::unique_ptr<Command> popNoWait();
  void setStatus(QueueStatus s) { this->queue_state.store(s); }
  QueueStatus getStatus() { return this->queue_state.load(); }
  bool empty() { return this->queue.empty(); }
};

/**
 * @brief high level request handling queue
 */
class CommQueue {
private:
  BlockingQueue request;/*! request queue: for async calls */
  BlockingQueue inflight;/*! in-flight queue: for reqs that have been submitted through URPC */
  BlockingQueue completion;/*! completion queue: finished reqs picked up from URPC */
public:
  CommQueue() {};

  int pushRequest(std::unique_ptr<Command>);
  void pushRequestFront(std::unique_ptr<Command>);
  std::unique_ptr<Command> popRequest();
  std::unique_ptr<Command> tryPopRequest();
  bool emptyRequest() { return this->request.empty(); }
  void pushInFlight(std::unique_ptr<Command>);
  bool emptyInFlight() { return this->inflight.empty(); }
  std::unique_ptr<Command> popInFlight();
  void pushCompletion(std::unique_ptr<Command>);
  std::unique_ptr<Command> waitCompletion(uint64_t msgid);
  std::unique_ptr<Command> peekCompletion(uint64_t msgid);
  void cancelAll();
  void setRequestStatus(QueueStatus s){ this->request.setStatus(s); }
};
} // namespace veo
#endif
