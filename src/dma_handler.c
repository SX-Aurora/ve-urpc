/* VE side DMA handler */

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>
#include "urpc_common.h"

// Flow:
// 1. insert decoded cmd
// 2. check handles if DMA done
// 3. submit to DMA -> link to handle (optimization: coalesce, multiple reqs: same handle)
// 4. call handler
//
// Insert and submit are separate because the submit can fail (EAGAIN).
//
// slot  +---
//    0  |     <- out         (last submitted)
//    1  |     <- dma_done    (last cmd with payload transfered)
//    2  |     
//    3  |     <- dma_submit  (last cmd with payload transfer over dma requested)
//    4  |     <- in          (last inserted cmd)
//    5  |
//       +---
//



void init_dma_handler(urpc_comm_t *uc)
{
	dma_handler_t *dh = &uc->dhq;

	dh->in = dh->submit = dh->done = dh->out = -1;
	dh->in_req = -1;
}

int64_t dhq_cmd_in(urpc_comm_t *uc, urpc_mb_t *m, int is_recv)
{
	dma_handler_t *dh = &uc->dhq;

	// incoming slot was free in the mb list, so it must be available here, too
	dh->in_req++;
	dh->in = REQ2SLOT(dh->in_req);
	dh->cmd[dh->in].u64 = m->u64;
        dprintf("dhq_cmd_in() %s cmd=%d req=%ld in=%d submit=%d done=%d out=%d\n",
		is_recv ? "recv" : "send", m->c.cmd, dh->in_req, dh->in, dh->submit, dh->done, dh->out);
	return dh->in_req;
}

static void report_exception_and_signal(dma_handler_t *dh, int slot, int exc)
{
	// 0x8000: Memory protection exception
	// 0x4000: Missing page exception
	// 0x2000: Missing space exception
	// 0x1000: Memory access exception
	// 0x0800: I/O access exception
	urpc_mb_t *m = &dh->cmd[slot];
	pid_t pid = getpid();
	eprintf("DMA encountered exception, rc=0x%x"
		" slot=%d cmd=%d offs=%ld len=%ld\n",
		exc, slot, m->c.cmd, m->c.offs, m->c.len);
	if (exc & 0x8000) {
		eprintf("memory protection exception\n");
		kill(pid, SIGBUS);
	} else if (exc & 0x4000) {
		eprintf("memory protection exception\n");
		kill(pid, SIGBUS);
	} else if (exc & 0x2000) {
		eprintf("missing space exception\n");
		kill(pid, SIGBUS);
	} else if (exc & 0x1000) {
		eprintf("memory access exception\n");
		kill(pid, SIGSEGV);
	} else if (exc & 0x0800) {
		eprintf("I/O access exception\n");
		kill(pid, SIGIOT);
	}
	kill(pid, SIGILL);
}

int dhq_cmd_check_done(urpc_comm_t *uc, int is_recv)
{
	int i, ncheck, rc, slot, start;
	dma_handler_t *dh = &uc->dhq;

	// check if any DMA handles have finished in the mean time
	// ... from dma_done + 1 to dma_submit
	ncheck = dh->submit - dh->done;
	if (dh->submit < dh->done)
		ncheck += URPC_LEN_MB;
        dprintf("dhq_cmd_check_done() %s ncheck=%d\n", is_recv ? "recv" : "send", ncheck);

	for (i = 0, start = dh->done + 1; i < ncheck; i++) {
		slot = REQ2SLOT(start + i);
		urpc_mb_t *m = &dh->cmd[slot];
		if (m->c.len == 0) {
			// this one had no DMA,
			dh->done = slot;
			continue;
		}
		// check DMA handle
		rc = ve_dma_poll(&dh->handle[slot]);
                dprintf("dhq_cmd_check_done() %s i=%d ve_dma_poll returned %d\n",
			is_recv ? "recv" : "send", i, rc);
		if (rc == -EAGAIN)
			break;		// not done, yet, no further checking
		else if (rc == 0) {
			// done
			dh->done = slot;
			continue;
		} else {
			// exception happened!
			// raise signal and die
			report_exception_and_signal(dh, slot, rc);
			break;
		}
	}
        dprintf("dhq_cmd_check_done() %s in=%d submit=%d done=%d out=%d\n",
		is_recv ? "recv" : "send", dh->in, dh->submit, dh->done, dh->out);
	return ncheck;
}

/**
 * @brief Submit as many decoded DMA payload transfer requests as possible.
 *
 * @param uc urpc communicator
 * @param is_recv flag, true if reading/receiving from VH
 */
int dhq_dma_submit(urpc_comm_t *uc, int is_recv)
{
	int i, ncheck, rc, size, slot, start;
	uint64_t src, dst;
	dma_handler_t *dh = &uc->dhq;

	// check if any DMA handles have finished in the mean time
	// ... from submit + 1 to in
	ncheck = dh->in - dh->submit;
	if (dh->in < dh->submit)
		ncheck += URPC_LEN_MB;
        dprintf("dhq_dma_submit() %s ncheck=%d\n", is_recv ? "recv" : "send", ncheck);

	// TODO: implement coalescing transfers!
	for (i = 0, start = dh->submit + 1; i < ncheck; i++) {
		slot = REQ2SLOT(start + i);
		urpc_mb_t *m = &dh->cmd[slot];
		if (m->c.len == 0) {
			// this one has no DMA, move ahead
			dh->submit = slot;
			continue;
		}
		if (is_recv) {
			dst = uc->mirr_data_vehva + m->c.offs;
			src = uc->shm_data_vehva + m->c.offs;
		} else {
			src = uc->mirr_data_vehva + m->c.offs;
			dst = uc->shm_data_vehva + m->c.offs;
		}
		size = m->c.len;
		rc = ve_dma_post(dst, src, size, &dh->handle[slot]);
                dprintf("dhq_dma_submit() %s i=%d ve_dma_post returned %d\n",
			is_recv ? "recv" : "send", i, rc);
		if (rc == -EAGAIN)
			break;
		// all went well! move pointer
		dh->submit = slot;
	}
        dprintf("dhq_dma_submit() %s in=%d submit=%d done=%d out=%d\n",
		is_recv ? "recv" : "send", dh->in, dh->submit, dh->done, dh->out);
	return ncheck;
}

/**
 * @brief Call handler on commands which have received their payload.
 *
 * @param uc urpc communicator
 * @return number of handled requests
 *
 */
int dhq_cmd_handle(urpc_peer_t *up, int is_recv)
{
	int i, ncheck, rc, slot, start;
	uint64_t src, dst;
	int64_t req;
	void *payload;
	size_t plen;
	urpc_comm_t *uc;
	dma_handler_t *dh;
	transfer_queue_t *tq;

	if (is_recv)
		uc = &up->recv;
	else
		uc = &up->send;

	dh = &uc->dhq;
	tq = uc->tq;

	// check if any DMA handles have finished in the mean time
	// ... from out + 1 to done
	ncheck = dh->done - dh->out;
	if (dh->done < dh->out)
		ncheck += URPC_LEN_MB;
        dprintf("dhq_cmd_handle() %s ncheck=%d\n", is_recv ? "recv" : "send", ncheck);

	for (i = 0, start = REQ2SLOT(dh->out + 1); i < ncheck; i++) {
		slot = REQ2SLOT(start + i);
		urpc_mb_t *m = &dh->cmd[slot];

		req = dh->in_req - ((int64_t)dh->in - (int64_t)slot);
		if (dh->in < slot)
			req -= URPC_LEN_MB;

		if (m->c.len > 0) {
			payload = (void *)((char *)uc->mirr_data_buff + m->c.offs);
			plen = m->c.len;
		} else {
			payload = NULL;
			plen = 0;
		}

		if (is_recv) {
			//
			// call handler
			//
			dprintf("dhq_cmd_handle() recv before handler for cmd %d\n", m->c.cmd);
			urpc_handler_func func = up->handler[m->c.cmd];
			if (func) {
				rc = func(up, m, req, payload, plen);
				dprintf("dhq_cmd_handle() recv after handler for cmd %d\n", m->c.cmd);
				if (rc) {
					eprintf("Warning: RPC handler %d returned %d\n",
						m->c.cmd, rc);
					// TODO: more error handling
				}
			}
			dh->out = slot;
			urpc_slot_done(tq, slot, m);
		} else {
			//
			// submit command to tq
			//
			dprintf("dhq_cmd_handle() send submitting to URPC "
				"cmd=%d offs=%d len=%d\n", m->c.cmd, m->c.offs, m->c.len);
			dh->out = slot;
			urpc_put_cmd(up, m);

		}
	}
        dprintf("dhq_cmd_handle() %s in=%d submit=%d done=%d out=%d\n",
		is_recv ? "recv" : "send", dh->in, dh->submit, dh->done, dh->out);
        return ncheck;
}

