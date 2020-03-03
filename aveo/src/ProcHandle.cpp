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
#include "urpc_time.h"

#include <string.h>
#include <unistd.h>
#include <sys/auxv.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/shm.h>
#ifdef _OPENMP
#include <omp.h>
#endif


namespace veo {

  bool _init_hooks_registered = false;
  
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
  auto rv = vh_urpc_child_create(this->up, binname, venode, 0);
  if (rv != 0) {
    throw VEOException("ProcHandle: failed to create VE process.");
    eprintf("ProcHandle: failed to create VE process.\n");
  }
  if (wait_peer_attach(this->up) != 0) {
    throw VEOException("ProcHandle: timeout while waiting for VE.");
  }

  this->mctx = new ThreadContext(this, this->up, true);
  //this->ctx.push_back(this->mctx);

  // The sync call returns the stack pointer from inside the VE kernel
  // call handler if the function address passed is 0.
  CallArgs args;
  auto rc = this->callSync(0, args, &this->ve_sp);
  if (rc < 0) {
    throw VEOException("ProcHandle: failed to get the VE SP.");
  }
  dprintf("proc stack pointer: %p\n", (void *)this->ve_sp);

  this->mctx->state = VEO_STATE_RUNNING;
  this->mctx->ve_sp = this->ve_sp;
  // first ctx gets core 0 (could be changed later)
  this->mctx->core = 0;
  this->ve_number = venode;
}

/**
 * @brief Exit veorun on VE side
 *
 */
int ProcHandle::exitProc()
{
  // TODO: proper lock
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
  VEO_TRACE(nullptr, "%s()", __func__);
  // TODO: send exit urpc command
  //
  auto req = urpc_generic_send(up, URPC_CMD_EXIT, (char *)"");
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
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
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
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
  this->mctx->_synchronize_nolock();
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
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
  this->mctx->_synchronize_nolock();
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
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
  this->mctx->_synchronize_nolock();
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
  VEO_TRACE(nullptr, "readMem(%p, %lx, %ld)", dst, src, size);
  auto req = this->mctx->asyncReadMem(dst, src, size);
  if (req == VEO_REQUEST_ID_INVALID) {
    eprintf("readMem failed! Aborting.");
    return -1;
  }
  uint64_t dummy;
  auto rv = this->mctx->callWaitResult(req, &dummy);
  return rv;
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
  VEO_TRACE(nullptr, "writeMem(%p, %lx, %ld)", dst, src, size);
  auto req = this->mctx->asyncWriteMem(dst, src, size);
  if (req == VEO_REQUEST_ID_INVALID) {
    eprintf("writeMem failed! Aborting.");
    return -1;
  }
  uint64_t dummy;
  auto rv = this->mctx->callWaitResult(req, &dummy);
  VEO_TRACE(nullptr, "writeMem leave... rv=%d", rv);
  return rv;
}

/**
 * @brief start a function on the VE, wait for result and return it.
 *
 * @param addr VEMVA of function called
 * @param args arguments of the function
 * @param result pointer to result
 * @return 0 if all went well.
 */
int ProcHandle::callSync(uint64_t addr, CallArgs &arg, uint64_t *result)
{
  return this->mctx->callSync(addr, arg, result);
}

/**
 * @brief Return number of contexts in the ctx vector
 *
 * @return size of ctx vector
 */
int ProcHandle::numContexts()
{
  return this->ctx.size();
}

/**
 * @brief Return pointer to a context in the ctx vector
 *
 * @param idx index / position inside the ctx vector
 *
 * @return pointer to ctx, raises exception if out of bounds.
 */
ThreadContext *ProcHandle::getContext(int idx)
{
  return this->ctx.at(idx).get();
}

/**
 * @brief Remove context from the ctx vector
 *
 * The context is kept in a smart pointer, therefore it will be deleted implicitly.
 * 
 */
void ProcHandle::delContext(ThreadContext *ctx)
{
  for (auto it = this->ctx.begin(); it != this->ctx.end(); it++) {
    if ((*it).get() == ctx) {
      (*it).get()->close();
      this->ctx.erase(it);
      break;
    }
  }
}

/**
 * @brief open a new context (VE thread)
 *
 * @return a new thread context created
 *
 * The first context returned is the this->mctx!
 */
ThreadContext *ProcHandle::openContext()
{
  std::lock_guard<std::mutex> lock(this->mctx->submit_mtx);
  this->mctx->_synchronize_nolock();

  if (this->ctx.empty()) {
    this->ctx.push_back(std::unique_ptr<ThreadContext>(this->mctx));
    return this->mctx;
  }
  
  // compute core
  int core = this->ctx.back()->core + 1;
#ifdef _OPENMP
  core += omp_get_num_threads() - 1;
#endif
  if (core >= MAX_VE_CORES) {
    eprintf("No more contexts allowed. You should have at  most one per VE core!\n");
    return nullptr;
  }
  
  // create vh side peer
  auto new_up = vh_urpc_peer_create();
  if (new_up == nullptr) {
    throw VEOException("ProcHandle: failed to create new VH side urpc peer.");
  }

  // start another thread inside peer proc that attaches to the new up
  auto req = urpc_generic_send(this->up, URPC_CMD_NEWPEER, (char *)"II",
                               new_up->shm_segid, core);

  if (wait_peer_attach(new_up) != 0) {
    throw VEOException("ProcHandle: timeout while waiting for VE.");
  }
  // wait for peer receiver to set flag to 1
  while(! (urpc_get_receiver_flags(&new_up->send) & 0x1) ) {
    busy_sleep_us(1000);
  }

  int64_t rc;
  wait_req_result(this->up, req, &rc);
  if (rc) {
    vh_urpc_peer_destroy(new_up);
    return nullptr;
  }

  auto new_ctx = new ThreadContext(this, new_up, false);
  this->ctx.push_back(std::unique_ptr<ThreadContext>(new_ctx));

  CallArgs args;
  auto rc2 = new_ctx->callSync(0, args, &new_ctx->ve_sp);
  if (rc2 < 0) {
    throw VEOException("ProcHandle: failed to get the new VE SP.");
  }
  dprintf("proc stack pointer: %p\n", (void *)new_ctx->ve_sp);

  new_ctx->state = VEO_STATE_RUNNING;
  return new_ctx;
}

} // namespace veo