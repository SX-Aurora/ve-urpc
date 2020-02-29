#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <csignal>

#include <urpc_common.h>
#include <urpc_debug.h>
#include <urpc_time.h>
#include <veo_urpc.hpp>

//#define _GNU_SOURCE
#include <dlfcn.h>


extern urpc_peer_t *main_up;

void signalHandler( int signum ) {
  Dl_info di;
  
  //eprintf("Interrupt signal %s received\n", strsignal(signum));
  eprintf("Interrupt signal %d received\n", signum);

  // try to print info about stack trace
  __builtin_traceback((unsigned long *)__builtin_frame_address(0));
  void *f = __builtin_return_address(0);
  if (f) {
    if (dladdr(f, &di)) {
      printf("%p -> %s\n", f, di.dli_sname);
    } else {
      printf("%p\n", f);
    }
    void *f = __builtin_return_address(1);
    if (f) {
      if (dladdr(f, &di)) {
        printf("%p -> %s\n", f, di.dli_sname);
      } else {
        printf("%p\n", f);
      }
      void *f = __builtin_return_address(2);
      if (f) {
        if (dladdr(f, &di)) {
          printf("%p -> %s\n", f, di.dli_sname);
        } else {
          printf("%p\n", f);
        }
      }
    }
  }
  urpc_generic_send(main_up, veo::URPC_CMD_EXCEPTION, (char *)"L", (int64_t)signum);
  sleep(20);
  ve_urpc_fini(main_up);
  exit(signum);  
}


urpc_peer_t *main_up;

extern "C" {
  extern int veo_finish_;
  //extern void veo_urpc_register_ve_handlers(urpc_peer_t *);
}

int main()
{
  int err;
  long ts = get_time_us();

  signal(SIGABRT, signalHandler);
  signal(SIGFPE, signalHandler);
  signal(SIGILL, signalHandler);
  signal(SIGSEGV, signalHandler);
  
  //urpc_set_handler_init_hook(&veo_urpc_register_ve_handlers);

  const char* env_p = getenv("VEO_MAXINFLIGHT");
  size_t maxinfl = 8;
  if (const char* env_p = getenv("VEO_MAXINFLIGHT"))
    maxinfl = atoi(env_p);

  urpc_peer_t *up = ve_urpc_init(0, -1);
  if (up == NULL)
    return -1;

  main_up = up;
  urpc_set_receiver_flags(&up->recv, 1);

  dprintf("VE: set receiver flag to 1.\n");

  while (!veo_finish_) {
    // carefull with number of progress calls
    // number * max_send_buff_size should not be larger than what we have
    // as send buffer memory
    err = ve_urpc_recv_progress(up, 10, maxinfl);
#ifdef DEBUGMEM
    if (timediff_us(ts) > 100000) {
      dhq_state(up);
      ts = get_time_us();
    }
#endif
  }

  ve_urpc_fini(up);
  return 0;
}
