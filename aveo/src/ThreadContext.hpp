/**
 * @file ThreadContext.hpp
 * @brief VEO thread context
 */
#ifndef _VEO_THREAD_CONTEXT_HPP_
#define _VEO_THREAD_CONTEXT_HPP_

#include "Command.hpp"
#include <mutex>
#include <unordered_set>
#include <pthread.h>
#include <semaphore.h>

#include <urpc_common.h>
#include <ve_offload.h>

namespace veo {

class ProcHandle;
class CallArgs;

/**
 * @brief VEO thread context
 */
class ThreadContext {
  friend class ProcHandle;// ProcHandle controls the main thread directly.
private:
  ProcHandle *proc;
  CommQueue comq;
  veo_context_state state;
  bool is_main_thread;
  uint64_t seq_no;
  uint64_t ve_sp;
  urpc_peer_t *up;		//!< ve-urpc peer pointer, each ctx has one
  std::unordered_set<uint64_t> rem_reqid;
  std::mutex req_mtx;

  void _progress_nolock(int ops);
  void progress(int ops);
  /**
   * @brief Issue a new request ID
   * @return a request ID, 64 bit integer, to identify a command
   */
  uint64_t issueRequestID() {
    uint64_t ret = VEO_REQUEST_ID_INVALID;
    while (ret == VEO_REQUEST_ID_INVALID) {
      ret = __atomic_fetch_add(&this->seq_no, 1, __ATOMIC_SEQ_CST);
    }
    std::lock_guard<std::mutex> lock(this->req_mtx);
    rem_reqid.insert(ret);
    return ret;
  }

  // handlers for commands
  int _readMem(void *, uint64_t, size_t);
  int _writeMem(uint64_t, const void *, size_t);
  uint64_t _callOpenContext(ProcHandle *, uint64_t, CallArgs &);
public:
  ThreadContext(ProcHandle *, urpc_peer_t *up);
  ThreadContext(ProcHandle *);
  ~ThreadContext() {};
  ThreadContext(const ThreadContext &) = delete;//non-copyable
  veo_context_state getState() { return this->state; }
  uint64_t callAsync(uint64_t, CallArgs &);
  uint64_t callAsyncByName(uint64_t, const char *, CallArgs &);
  uint64_t callVHAsync(uint64_t (*)(void *), void *);
  int callWaitResult(uint64_t, uint64_t *);
  int callPeekResult(uint64_t, uint64_t *);
  //uint64_t asyncReadMem(void *, uint64_t, size_t);
  //uint64_t asyncWriteMem(uint64_t, const void *, size_t);

  veo_thr_ctxt *toCHandle() {
    return reinterpret_cast<veo_thr_ctxt *>(this);
  }
  bool isMainThread() { return this->is_main_thread;}
  int64_t _closeCommandHandler(uint64_t id);
  int close();

};

} // namespace veo
#endif
