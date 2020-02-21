#include <stdio.h>
#include <cstdint>
#include <cstdlib>
#include <dlfcn.h>

#include "urpc_common.h"
#include "veo_urpc.h"

extern "C" {

int veo_finish_ = 0;

	//
	// Commands
	//

void send_ping_nolock(urpc_peer_t *up)
{
	urpc_mb_t m;
	m.c.cmd = URPC_CMD_PING;
	m.c.offs=0; m.c.len=0;
	int64_t req = urpc_put_cmd(up, &m);
	if (req < 0)
		eprintf("send_ping failed\n");
}

int64_t send_ack_nolock(urpc_peer_t *up)
{
	return send_cmd_nopayload(up, URPC_CMD_ACK);
}

int64_t send_result_nolock(urpc_peer_t *up, int64_t result)
{
	return urpc_generic_send(up, URPC_CMD_RESULT, (char *)"L", result);
}

int64_t send_unloadlib_nolock(urpc_peer_t *up, const uint64_t libhndl)
{
	return urpc_generic_send(up, URPC_CMD_UNLOADLIB, (char *)"L", libhndl);
}

/**
 * @brief Read VE memory
 *
 * @param up URPC peer
 * @param src VE source address of full transfer
 * @param size the full transfer size
 * @return request ID if successful, -1 if failed.
 */
int64_t send_read_mem_nolock(urpc_peer_t *up, uint64_t src, size_t size)
{
	return urpc_generic_send(up, URPC_CMD_RECVBUFF, (char *)"LL", src, size);
}

/**
 * @brief Write VE memory
 *
 * @param up URPC peer
 * @param dst VE destination address of full transfer (data could follow in multiple fragments)
 * @param size the full (possibly multi-part) transfer size
 * @param buff all or only a fragment of the transferred data
 * @param bsz size of data fragment transported with this message
 * @return request ID if successful, -1 if failed.
 */
int64_t send_write_mem_nolock(urpc_peer_t *up, uint64_t dst, size_t size, void *buff, size_t bsz)
{
	return urpc_generic_send(up, URPC_CMD_SENDBUFF, (char *)"LLP",
				 dst, size, buff, bsz);
}

/**
 * @brief Send buffer fragment
 *
 * @param up URPC peer
 * @param src address of buffer fragment
 * @param size of buffer fragment
 * @return request ID if successful, -1 if failed.
 */
int64_t send_sendfrag_nolock(urpc_peer_t *up, void *src, size_t size)
{
	return urpc_generic_send(up, URPC_CMD_SENDFRAG, (char *)"LP",
				 (uint64_t)src, src, size);
}

int wait_req_result(urpc_peer_t *up, int64_t req, int64_t *result)
{
	// wait for result
	urpc_mb_t m;
	void *payload;
	size_t plen;
	if (!urpc_recv_req_timeout(up, &m, req, REPLY_TIMEOUT, &payload, &plen)) {
		// timeout! complain.
		eprintf("timeout waiting for RESULT req=%ld\n", req);
		return -1;
	}
	if (m.c.cmd == URPC_CMD_RESULT) {
		if (plen) {
			urpc_unpack_payload(payload, plen, (char *)"L", result);
                        urpc_slot_done(up->recv.tq, REQ2SLOT(req), &m);
		} else {
			dprintf("result message for req=%ld had no payload!?", req);
		}
	} else {
		eprintf("unexpected RESULT message type: %d\n", m.c.cmd);
		return -1;
	}
	return 0;
}

int wait_req_ack(urpc_peer_t *up, int64_t req)
{
	// wait for result
	urpc_mb_t m;
	void *payload;
	size_t plen;
	if (!urpc_recv_req_timeout(up, &m, req, REPLY_TIMEOUT, &payload, &plen)) {
		// timeout! complain.
		eprintf("timeout waiting for ACK req=%ld\n", req);
		return -1;
	}
	if (m.c.cmd != URPC_CMD_ACK) {
		eprintf("unexpected ACK message type: %d\n", m.c.cmd);
		return -2;
	}
        urpc_slot_done(up->recv.tq, REQ2SLOT(req), &m);
	return 0;
}

	//
	// Handlers
	//

static int ping_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			void *payload, size_t plen)
{
	send_ack_nolock(up);
	return 0;
}

static int exit_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			void *payload, size_t plen)
{
	veo_finish_ = 1;
	send_ack_nolock(up);
	return 0;
}

static int loadlib_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                           void *payload, size_t plen)
{
	size_t psz;
	char *libname;

	urpc_unpack_payload(payload, plen, (char *)"P", &libname, &psz);

	uint64_t handle = (uint64_t)dlopen(libname, RTLD_NOW);
	dprintf("loadlib_handler libname=%s handle=%p\n", libname, handle);

	int64_t new_req = send_result_nolock(up, handle);
	// check req IDs. Result expected with exactly same req ID.
	if (new_req != req) {
		eprintf("loadlib_handler: send result req ID mismatch: %ld instead of %ld\n",
		       new_req, req);
		return -1;
	}
	return 0;
}

static int unloadlib_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			     void *payload, size_t plen)
{
	size_t psz;
	uint64_t libhndl;

	urpc_unpack_payload(payload, plen, (char *)"L", &libhndl);

	int rc = dlclose((void *)libhndl);

	int64_t new_req = send_result_nolock(up, (int64_t)rc);
	if (new_req != req || new_req < 0) {
		eprintf("unloadlib_handler: send result failed\n");
		return -1;
	}
	return 0;
}

static int getsym_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			  void *payload, size_t plen)
{
	uint64_t libhndl;
	char *sym;
	size_t psz;
	uint64_t symaddr = 0;

	urpc_unpack_payload(payload, plen, (char *)"LP", &libhndl, &sym, &psz);
	if (libhndl)
		symaddr = (uint64_t)dlsym((void *)libhndl, sym);
#if 0
	typedef struct {char *n; void *v;} static_sym_t;

	if (_veo_static_symtable) {
		static_sym_t *t = _veo_static_symtable;
		while (t->n != NULL) {
			if (strcmp(t->n, name) == 0) {
				symaddr = (uint64_t)t->v;
				break;
			}
			t++;
		}
	}
#endif
	
	int64_t new_req = send_result_nolock(up, symaddr);
	// check req IDs. Result expected with exactly same req ID.
	if (new_req != req) {
		printf("getsym_handler: send result req ID mismatch: %ld instead of %ld\n",
		       new_req, req);
		return -1;
	}
	return 0;
}

static int alloc_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			 void *payload, size_t plen)
{
	size_t psz;
	size_t allocsz;

	urpc_unpack_payload(payload, plen, (char *)"L", &allocsz);

	void *addr = malloc(allocsz);
	dprintf("alloc_handler addr=%p size=%lu\n", addr, allocsz);

	int64_t new_req = send_result_nolock(up, (uint64_t)addr);
	// check req IDs. Result expected with exactly same req ID.
	if (new_req != req) {
		eprintf("alloc_handler: send result req ID mismatch: %ld instead of %ld\n",
		       new_req, req);
		return -1;
	}
	return 0;
}

static int free_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			void *payload, size_t plen)
{
	size_t psz;
	uint64_t addr;

	urpc_unpack_payload(payload, plen, (char *)"L", &addr);

	free((void *)addr);
	dprintf("free_handler addr=%p\n", (void *)addr);

	int64_t new_req = send_ack_nolock(up);
	// check req IDs. Result expected with exactly same req ID.
	if (new_req != req) {
		eprintf("free_handler: send result req ID mismatch: %ld instead of %ld\n",
		       new_req, req);
		return -1;
	}
	return 0;
}


static int readmem_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
                           void *payload, size_t plen)
{
	uint64_t src;
	size_t size;

	urpc_unpack_payload(payload, plen, (char *)"LL", &src, &size);
	dprintf("readmem_handler src=%p size=%lu\n", (void *)src, size);

        // TODO: lock send comm
        char *s = (char *)src;
	int acks = 0;
        while (size > 0) {
		size_t psz;
		psz = size <= MAX_SENDFRAG ? size : PART_SENDFRAG;
		dprintf("readmem_handler psz=%ld\n", psz);
		auto new_req = urpc_generic_send(up, URPC_CMD_SENDFRAG, (char *)"P",
						 (void *)s, psz);
			
		// check req IDs. Result expected with exactly same req ID.
		if (new_req != req) {
			eprintf("readmem_handler: send result req ID mismatch:"
				" %ld instead of %ld\n", new_req, req);
			return -1;
		}
		size -= psz;
		s += psz;
		++req;
		if (size > 0)
			++acks;
		if (acks > 100) {
			if (pickup_acks(up, acks) == 0)
				acks = 0;
			else
				return -1;
		}
	}
	// pick up ACKs lazily
	if (acks)
		if (pickup_acks(up, acks) != 0)
			return -1;
	return 0;
}

static int writemem_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			    void *payload, size_t plen)
{
	uint64_t dst;
	size_t size;
	void *buff;
	size_t bsz;

	urpc_unpack_payload(payload, plen, (char *)"LLP", &dst, &size, &buff, &bsz);
	dprintf("writemem_handler dst=%p size=%lu buffsz=%ld\n", (void *)dst, size, bsz);

        char *d = (char *)dst;
	memcpy(d, buff, bsz);
	urpc_slot_done(up->recv.tq, REQ2SLOT(req), m);
	send_ack_nolock(up);
	size -= bsz;
	d += bsz;

        // TODO: lock send comm ?
        while (size > 0) {
		urpc_mb_t m;
		req += 1;
		if (!urpc_recv_req_timeout(up, &m, req, REPLY_TIMEOUT, &payload, &plen)) {
			// timeout! complain.
			eprintf("writemem_handler timeout waiting for SENDFRAG req=%ld\n", req);
			return -1;
		}
		// send an ACK to keep req IDs in sync
		send_ack_nolock(up);
		if (m.c.cmd != URPC_CMD_SENDFRAG) {
			eprintf("expected SENDFRAG message, got: %d\n", m.c.cmd);
			return -1;
		}
		if (plen) {
			urpc_unpack_payload(payload, plen, (char *)"P", &buff, &bsz);
			memcpy(d, buff, bsz);
			urpc_slot_done(up->recv.tq, REQ2SLOT(req), &m);
			size -= bsz;
			d += bsz;
		} else {
			dprintf("result message for req=%ld had no payload!?", req);
		}
	}
	return 0;
}

#ifdef __ve__

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

	if (m->c.cmd == URPC_CMD_CALL) {
		flags = 0;
		urpc_unpack_payload(payload, plen, (char *)"LP", &addr, (void **)&regs, &nregs);
		nregs = nregs / 8;

		//
		// if addr == 0: send current stack pointer
		//
		if (addr == 0) {
			int64_t new_req = send_result_nolock(up, curr_sp);
			if (new_req != req || new_req < 0) {
				eprintf("call_handler sp send failed, req mismatch.\n");
				return -1;
			}
			return 0;
		}
                dprintf("call_handler: no stack, nregs=%d\n", nregs);

	} else if (m->c.cmd == URPC_CMD_CALL_STKIN ||
		   m->c.cmd == URPC_CMD_CALL_STKINOUT) {
		flags |= VEO_CALL_CACHE_IN;
	} else if (m->c.cmd == URPC_CMD_CALL_STKINOUT) {
		flags |= VEO_CALL_CACHE_OUT;
	}
	if (flags & VEO_CALL_CACHE_IN) {
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
	if (flags & VEO_CALL_CACHE_IN) {
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
	} else {
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

	if (flags & VEO_CALL_CACHE_OUT) {
		// copying back from stack must happen in the same function,
		// otherwise we overwrite it!
#pragma _NEC ivdep
		for (int i = 0; i < stack_size / 8; i++) {
			((uint64_t *)stack)[i] = ((uint64_t *)stack_top)[i];
		}
		int64_t new_req = urpc_generic_send(up, URPC_CMD_RESCACHE, (char *)"LP",
						    result, stack, stack_size);
		if (new_req != req || new_req < 0) {
			eprintf("call_handler sp send failed, req mismatch.\n");
			return -1;
		}
	} else {
		int64_t new_req = send_result_nolock(up, result);
		if (new_req != req || new_req < 0) {
			eprintf("call_handler: send result failed\n");
			return -1;
		}
	}
	return 0;
}


static int get_sp_handler(urpc_peer_t *up, urpc_mb_t *m, int64_t req,
			  void *payload, size_t plen)
{
	int64_t sp;

	asm volatile ("or %0, 0, %sp": "=r"(sp));

	int64_t new_req = send_result_nolock(up, sp);
	if (new_req != req || new_req < 0) {
		eprintf("get_sp_handler: send result failed\n");
		return -1;
	}
	return 0;
}
#endif

#ifdef __ve__
void veo_urpc_register_ve_handlers(urpc_peer_t *up)
{
	int err;

	if ((err = urpc_register_handler(up, URPC_CMD_PING, &ping_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_EXIT, &exit_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_LOADLIB, &loadlib_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_UNLOADLIB, &unloadlib_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_GETSYM, &getsym_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_ALLOC, &alloc_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_FREE, &free_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_RECVBUFF, &readmem_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_SENDBUFF, &writemem_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_CALL, &call_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_CALL_STKIN, &call_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
	if ((err = urpc_register_handler(up, URPC_CMD_CALL_STKINOUT, &call_handler)) < 0)
		eprintf("register_handler failed for cmd %d\n", 1);
}
#endif
} // extern "C"

#ifdef __ve__
__attribute__((constructor))
static void _veo_urpc_init_register(void)
{
	urpc_set_handler_init_hook(&veo_urpc_register_ve_handlers);
}
#endif

