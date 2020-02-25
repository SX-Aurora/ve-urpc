/**
 * @file ProcHandle.cpp
 * @brief implementation of ProcHandle
 */
#include "ProcHandle.hpp"
//#include "ThreadContext.hpp"
#include "VEOException.hpp"
#include "CallArgs.hpp"
#include "log.hpp"

#include "veo_urpc.hpp"

#include <string.h>
#include <unistd.h>
#include <sys/auxv.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/shm.h>

namespace veo {
  
/**
 * @brief constructor
 *
 * @param venode VE node ID for running child peer
 * @param binname VE executable
 */
ProcHandle::ProcHandle(int venode, char *binname) : ve_number(-1)
{
  // create vh side peer
  this->up = vh_urpc_peer_create();
  if (this->up == nullptr) {
    throw VEOException("ProcHandle: failed to create VH side urpc peer.");
  }

  // create VE process connected to this peer
  auto rv = vh_urpc_child_create(this->up, binname, venode, -1);
  if (rv != 0) {
    throw VEOException("ProcHandle: failed to create VE process.");
    eprintf("ProcHandle: failed to create VE process.\n");
  }
  if (wait_peer_attach(this->up) != 0) {
    throw VEOException("ProcHandle: timeout while waiting for VE.");
  }

  this->main_ctx = new ThreadContext(this, this->up);
  this->ctx.push_back(this->main_ctx);

  // The sync call returns the stack pointer from inside the VE kernel
  // call handler if the function address passed is 0.
  CallArgs args;
  auto rc = this->callSync(0, args, &this->ve_sp);
  if (rc < 0) {
    throw VEOException("ProcHandle: failed to get the VE SP.");
  }
  dprintf("proc stack pointer: %p\n", (void *)this->ve_sp);

  this->main_ctx->state = VEO_STATE_RUNNING;

  this->ve_number = venode;
}

/**
 * @brief Exit veorun on VE side
 *
 */
int ProcHandle::exitProc()
{
  // TODO: proper lock
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  VEO_TRACE(nullptr, "%s()", __func__);
  // TODO: send exit urpc command
  //
  auto req = send_cmd_nopayload(this->up, URPC_CMD_EXIT);
  if (req < 0) {
    throw VEOException("exitProc: failed to send EXIT cmd.");
  }
  auto rc = wait_req_ack(this->up, req);
  if (rc < 0) {
    // failed the smooth way, now kill the VE process
    rc = vh_urpc_child_destroy(this->up);
    if (rc) {
      // just print message if failed, but continue
      VEO_ERROR(nullptr, "failed to destroy VE child (rc=%d)", rc);
    }
  }
  rc = vh_urpc_peer_destroy(this->up);
  this->ve_number = -1;
  return rc;
}

/**
 * @brief Load a VE library in VE process space
 *
 * @param libname a library name
 * @return handle of the library loaded upon success; zero upon failure.
 */
uint64_t ProcHandle::loadLibrary(const char *libname)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  VEO_TRACE(nullptr, "%s(%s)", __func__, libname);
  size_t len = strlen(libname);
  if (len > VEO_SYMNAME_LEN_MAX) {
    throw VEOException("Library name too long", ENAMETOOLONG);
  }

  // lock peer (not needed any more because using proc mutex)

  // send loadlib cmd
  uint64_t req = urpc_generic_send(up, URPC_CMD_LOADLIB, (char *)"P",
                                   libname, (size_t)strlen(libname));

  // wait for result
  uint64_t handle = 0;
  wait_req_result(this->up, req, (int64_t *)&handle);

  // unlock peer (not needed any more)

  VEO_TRACE(nullptr, "handle = %#lx", handle);
  return handle;
}

/**
 * @brief Find a symbol in VE program
 *
 * @param libhdl handle of library
 * @param symname a symbol name to find
 * @return VEMVA of the symbol upon success; zero upon failure.
 */
uint64_t ProcHandle::getSym(const uint64_t libhdl, const char *symname)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  size_t len = strlen(symname);
  if (len > VEO_SYMNAME_LEN_MAX) {
    throw VEOException("Too long name", ENAMETOOLONG);
  }
  sym_mtx.lock();
  auto sym_pair = std::make_pair(libhdl, symname);
  auto itr = sym_name.find(sym_pair);
  if( itr != sym_name.end() ) {
    sym_mtx.unlock();
    VEO_TRACE(nullptr, "symbol addr = %#lx", itr->second);
    VEO_TRACE(nullptr, "symbol name = %s", symname);
    return itr->second;
  }
  sym_mtx.unlock();
  
  // lock peer (not needed any more because using proc mutex)

  uint64_t req = urpc_generic_send(up, URPC_CMD_GETSYM, (char *)"LP",
                                   libhdl, symname, (size_t)strlen(symname));

  uint64_t symaddr = 0;
  wait_req_result(this->up, req, (int64_t *)&symaddr);

  // unlock peer (not needed any more)

  VEO_TRACE(nullptr, "symbol addr = %#lx", symaddr);
  VEO_TRACE(nullptr, "symbol name = %s", symname);
  if (symaddr == 0) {
    return symaddr;
  }
  sym_mtx.lock();
  sym_name[sym_pair] = symaddr;
  sym_mtx.unlock();
  return symaddr;
}

/**
 * @brief Allocate a buffer on VE
 *
 * @param size of buffer
 * @return VEMVA of the buffer upon success; zero upon failure.
 */
uint64_t ProcHandle::allocBuff(const size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  uint64_t req = urpc_generic_send(up, URPC_CMD_ALLOC, (char *)"L", size);

  uint64_t addr = 0;
  wait_req_result(this->up, req, (int64_t *)&addr);
  return addr;
}

/**
 * @brief Free a buffer on VE
 *
 * @param buff VEMVA of the buffer
 * @return nothing
 */
void ProcHandle::freeBuff(const uint64_t buff)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  uint64_t req = urpc_generic_send(up, URPC_CMD_FREE, (char *)"L", buff);
  wait_req_ack(this->up, req);
}

/**
 * @brief read data from VE memory
 * @param[out] dst buffer to store the data
 * @param src VEMVA to read
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ProcHandle::readMem(void *dst, uint64_t src, size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  VEO_TRACE(nullptr, "readMem(%p, %#lx, %ld)", dst, src, size);

  auto req = send_read_mem_nolock(this->up, src, size);

  char *d = (char *)dst;
  while (size > 0) {
    urpc_mb_t m;
    void *payload;
    size_t plen, bsz;
    void *buff;
    if (!urpc_recv_req_timeout(up, &m, req, REPLY_TIMEOUT, &payload, &plen)) {
      // timeout! complain.
      eprintf("readMem timeout waiting for SENDFRAG req=%ld\n", req);
      return -1;
    }
    if (m.c.cmd != URPC_CMD_SENDFRAG) {
      eprintf("expected SENDFRAG message, got: %d\n", m.c.cmd);
      return -1;
    }
    if (plen) {
      urpc_unpack_payload(payload, plen, (char *)"P", &buff, &bsz);
      memcpy(d, buff, bsz);
      urpc_slot_done(this->up->recv.tq, REQ2SLOT(req), &m);
      size -= bsz;
      d += bsz;
      ++req;
      if (size > 0) {
        // send an ACK to keep req IDs in sync
        send_ack_nolock(this->up);
      }
    } else {
      dprintf("result message for req=%ld had no payload!?", req);
    }
  }
  return 0;
}

/**
 * @brief write data to VE memory
 * @param dst VEMVA to write the data
 * @param src buffer holding data to write
 * @param size size to transfer in byte
 * @return zero upon success; negative upon failure
 */
int ProcHandle::writeMem(uint64_t dst, const void *src, size_t size)
{
  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  VEO_TRACE(nullptr, "writeMem(%#lx, %p, %ld)", dst, src, size);

  size_t psz;
  psz = size <= MAX_SENDFRAG ? size : PART_SENDFRAG;
  char *s = (char *)src;
  int acks = 0;

  int64_t req = urpc_generic_send(this->up, URPC_CMD_SENDBUFF, (char *)"LLP",
                                  dst, size, (void *)s, psz);
  size -= psz;
  s += psz;
  ++acks;

  while (size > 0) {
    req += 1;
    psz = size <= MAX_SENDFRAG ? size : PART_SENDFRAG;
    dprintf("writeMem psz=%ld (next req should be %ld\n", psz, req);
    auto new_req = urpc_generic_send(this->up, URPC_CMD_SENDFRAG, (char *)"P",
                                     (void *)s, psz);
    ++acks;
			
    // check req IDs. Result expected with exactly same req ID.
    if (new_req != req) {
      eprintf("writeMem: send result req ID mismatch:"
              " %ld instead of %ld\n", new_req, req);
      return -1;
    }
    size -= psz;
    s += psz;
    if (acks > 100) {
      if (pickup_acks(this->up, acks) == 0)
        acks = 0;
      else
        return -1;
    }
  }
  // pick up ACKs lazily
  if (acks)
    if (pickup_acks(this->up, acks) != 0)
      return -1;
  return 0;
}

/**
 * @brief start a function on the VE, wait for result and return it.
 *
 * @param addr VEMVA of function called
 * @param args arguments of the function
 * @param result pointer to result
 * @return 0 if all went well.
 */
int ProcHandle::callSync(uint64_t addr, CallArgs &args, uint64_t *result)
{
  urpc_mb_t m;
  void *payload;
  size_t plen;

  std::lock_guard<std::mutex> lock(this->main_ctx->submit_mtx);
  this->main_ctx->_synchronize_nolock();
  VEO_TRACE(nullptr, "%s(%#lx, ...)", __func__, addr);
  VEO_DEBUG(nullptr, "VE function = %p", (void *)addr);

  int64_t req = send_call_nolock(this->up, this->ve_sp, addr, args);

  // TODO: make sync call timeout configurable
  if (!urpc_recv_req_timeout(up, &m, req, 150*REPLY_TIMEOUT, &payload, &plen)) {
    // timeout! complain.
    eprintf("callSync timeout waiting for RESULT req=%ld\n", req);
    return -1;
  }

  int rc = unpack_call_result(&m, &args, payload, plen, result);
  
  urpc_slot_done(this->up->recv.tq, REQ2SLOT(req), &m);

  return rc;
}

/**
 * @brief open a new context (VE thread)
 *
 * @return a new thread context created
 *
 * The first context returned is the main_ctx!
 */
ThreadContext *ProcHandle::openContext()
{
  std::lock_guard<std::mutex> lock(this->main_mutex);

  // TODO: open other contexts
  return this->main_ctx;
}
 
} // namespace veo
