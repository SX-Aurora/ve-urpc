/**
 * @file ProcHandle.hpp
 * @brief VEO process handle
 */
#ifndef _VEO_PROC_HANDLE_HPP_
#define _VEO_PROC_HANDLE_HPP_
#include <unordered_map>
#include <utility>
#include <memory>
#include <mutex>
#include <iostream>

#include "ve_offload.h"
#include <urpc_common.h>
//#include <veorun.h>
#include "CallArgs.hpp"
//#include "ThreadContext.hpp"
#include "VEOException.hpp"

namespace std {
    template <>
    class hash<std::pair<uint64_t, std::string>> {
    public:
        size_t operator()(const std::pair<uint64_t, std::string>& x) const{
            return hash<uint64_t>()(x.first) ^ hash<std::string>()(x.second);
        }
    };
}

namespace veo {

/**
 * @brief VEO process handle
 */
class ProcHandle {
private:
  std::unordered_map<std::pair<uint64_t, std::string>, uint64_t> sym_name;
  std::mutex sym_mtx;
  std::mutex main_mutex;//!< acquire while using main_thread
  int peer_id;		//!< ve-urpc peer ID
  urpc_peer_t *up;	//!< ve-urpc peer pointer
  uint64_t ve_sp;       //!< stack pointer on VE side
  int ve_number;

public:
  ProcHandle(int, char *);
  ~ProcHandle() { if (this->ve_number >= 0) this->exitProc(); }

  uint64_t loadLibrary(const char *);
  uint64_t getSym(const uint64_t, const char *);

  uint64_t allocBuff(const size_t);
  void freeBuff(const uint64_t);

  int readMem(void *, uint64_t, size_t);
  int writeMem(uint64_t, const void *, size_t);
  int exitProc(void);

  int callSync(uint64_t, CallArgs &, uint64_t *);

  //ThreadContext *openContext();
  
  veo_proc_handle *toCHandle() {
    return reinterpret_cast<veo_proc_handle *>(this);
  }

  int veNumber() { return this->ve_number; }
};
} // namespace veo
#endif
