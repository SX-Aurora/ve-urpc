#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <CallArgs.hpp>
#include "veo_urpc.hpp"

#ifdef __cplusplus
extern "C" {
#endif

using namespace veo;

//
// Handlers
//

static void print_dhq_state(urpc_peer_t *up)
{
  dma_handler_t *dh = &up->recv.dhq;
  printf("dhq recv: in_req=%ld in=%d submit=%d done=%d out=%d\n",
         dh->in_req, dh->in, dh->submit, dh->done, dh->out);
  int64_t last_put = TQ_READ64(up->recv.tq->last_put_req);
  int64_t last_get = TQ_READ64(up->recv.tq->last_get_req);
  printf("recv last_put=%ld last_get=%ld\n", last_put, last_get);
  dh = &up->send.dhq;
  printf("dhq send: in_req=%ld in=%d submit=%d done=%d out=%d\n",
         dh->in_req, dh->in, dh->submit, dh->done, dh->out);
  last_put = TQ_READ64(up->send.tq->last_put_req);
  last_get = TQ_READ64(up->send.tq->last_get_req);
  printf("send last_put=%ld last_get=%ld\n", last_put, last_get);
}

static int call_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                        void *payload, size_t plen)
{
  void *stack = NULL;
  size_t stack_size = 0;
  uint64_t stack_top, recv_sp = 0, curr_sp;
  uint64_t addr = 0;
  uint64_t *regs, result;
  size_t nregs;
  int flags;

  asm volatile ("or %0, 0, %sp": "=r"(curr_sp));

  dprintf("call_handler cmd=%d\n", m->c.cmd);
  
  if (m->c.cmd == URPC_CMD_CALL) {
    flags = 0;
    urpc_unpack_payload(payload, plen, (char *)"LP", &addr, (void **)&regs, &nregs);
    nregs = nregs / 8;
    dprintf("call_handler CALL nregs=%d\n", nregs);

    //
    // if addr == 0: send current stack pointer
    //
    if (addr == 0) {
      dprintf("call_handler sending stack pointer value sp=%lx\n", curr_sp);
      int64_t new_req = urpc_generic_send(up, URPC_CMD_RESULT, (char *)"L", curr_sp);
      if (new_req != req || new_req < 0) {
        eprintf("call_handler sp send failed, req mismatch. Expected %ld got %ld\n",
                req, new_req);
        return -1;
      }
      return 0;
    }
    dprintf("call_handler: no stack, nregs=%d\n", nregs);

  } else if (m->c.cmd == URPC_CMD_CALL_STKINOUT) {
    flags |= VEO_CALL_STK_OUT | VEO_CALL_STK_IN;
  } else if (m->c.cmd == URPC_CMD_CALL_STKIN) {
    flags |= VEO_CALL_STK_IN;
  }
  if (flags & VEO_CALL_STK_IN) {
    urpc_unpack_payload(payload, plen, (char *)"LPLLP",
                        &addr, (void **)&regs, &nregs,
                        &stack_top, &recv_sp,
                        &stack, &stack_size);
    nregs = nregs / 8;
    dprintf("call_handler: stack IN, nregs=%d, stack_top=%p\n",
            nregs, (void *)stack_top);
  }
  //
  // check if sent stack pointer is the same as the current one
  //
  asm volatile ("or %0, 0, %sp": "=r"(curr_sp));

  if (recv_sp && recv_sp != curr_sp) {
    eprintf("call_handler stack pointer mismatch! "
            "curr=%p expected=%p\n", curr_sp, recv_sp);
    return -1;
  }
  //
  // prepare "fake" stack for current function, which enables us to pass
  // parameters and variables that look like local variables on this
  // function's stack.
  //
  if (flags & VEO_CALL_STK_IN) {
    memcpy((void *)stack_top, stack, stack_size);
  }
  //
  // set up registers
  //
#define SREG(N,V) asm volatile ("or %s" #N ", 0, %0"::"r"(V))
  if (nregs == 1) {
    SREG(0,regs[0]);
  } else if (nregs == 2) {
    SREG(0,regs[0]); SREG(1,regs[1]);
  } else if (nregs == 3) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]);
  } else if (nregs == 4) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]); SREG(3,regs[3]);
  } else if (nregs == 5) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]); SREG(3,regs[3]);
    SREG(4,regs[4]);
  } else if (nregs == 6) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]); SREG(3,regs[3]);
    SREG(4,regs[4]); SREG(5,regs[5]);
  } else if (nregs == 7) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]); SREG(3,regs[3]);
    SREG(4,regs[4]); SREG(5,regs[5]); SREG(6,regs[6]);
  } else if (nregs >= 8) {
    SREG(0,regs[0]); SREG(1,regs[1]); SREG(2,regs[2]); SREG(3,regs[3]);
    SREG(4,regs[4]); SREG(5,regs[5]); SREG(6,regs[6]); SREG(7,regs[7]);
  }
#undef SREG
  //
  // And now we call the function!
  //
  if (flags == 0) {
    asm volatile("or %s12, 0, %0\n\t"          /* target function address */
                 ::"r"(addr));
    asm volatile("bsic %lr, (,%s12)":::);
  } else if (flags & VEO_CALL_STK_OUT) {
    asm volatile("or %s12, 0, %0\n\t"          /* target function address */
                 "st %fp, 0x0(,%sp)\n\t"       /* save original fp */
                 "st %lr, 0x8(,%sp)\n\t"       /* fake prologue */
                 "st %got, 0x18(,%sp)\n\t"
                 "st %plt, 0x20(,%sp)\n\t"
                 "or %fp, 0, %sp\n\t"          /* switch fp to new frame */
                 "or %sp, 0, %1"               /* switch sp to new frame */
                 ::"r"(addr), "r"(stack_top));
    asm volatile("bsic %lr, (,%s12)":::);
    asm volatile("or %sp, 0, %fp\n\t"          /* restore original sp */
                 "ld %fp, 0x0(,%sp)"           /* restore original fp */
                 :::);
  }
  asm volatile("or %0, 0, %s0":"=r"(result));

  if (flags & VEO_CALL_STK_OUT) {
    // copying back from stack must happen in the same function,
    // otherwise we overwrite it!
#pragma _NEC ivdep
    for (int i = 0; i < stack_size / 8; i++) {
      ((uint64_t *)stack)[i] = ((uint64_t *)stack_top)[i];
    }
    dprintf("call_handler sending RESCACHE\n");
    int64_t new_req = urpc_generic_send(up, URPC_CMD_RESCACHE, (char *)"LP",
                                        result, stack, stack_size);
    if (new_req != req || new_req < 0) {
      eprintf("call_handler send RESCACHE failed, req mismatch. Expected %ld got %ld\n",
              req, new_req);
      print_dhq_state(up);
      return -1;
    }
  } else {
    dprintf("call_handler sending RESULT\n");
    int64_t new_req = urpc_generic_send(up, URPC_CMD_RESULT, (char *)"L", result);
    if (new_req != req || new_req < 0) {
      eprintf("call_handler: send RESULT failed. Expected %ld got %ld\n",
              req, new_req);
      print_dhq_state(up);
      return -1;
    }
  }
  return 0;
}

void veo_urpc_register_ve_handlers(urpc_peer_t *up)
{
  int err;

  if ((err = urpc_register_handler(up, URPC_CMD_CALL, &call_handler)) < 0)
    eprintf("register_handler failed for cmd %d\n", 1);
  if ((err = urpc_register_handler(up, URPC_CMD_CALL_STKIN, &call_handler)) < 0)
    eprintf("register_handler failed for cmd %d\n", 1);
  if ((err = urpc_register_handler(up, URPC_CMD_CALL_STKINOUT, &call_handler)) < 0)
    eprintf("register_handler failed for cmd %d\n", 1);
}

__attribute__((constructor))
static void _veo_urpc_init_register(void)
{
  dprintf("registering VE URPC handlers\n");
  urpc_set_handler_init_hook(&veo_urpc_register_ve_handlers);
}

#ifdef __cplusplus
} //extern "C"
#endif
