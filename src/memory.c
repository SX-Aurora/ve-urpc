#include "urpc_common.h"
#include "urpc_time.h"
#include "ve_inst.h"


static inline void _report_free(urpc_comm_t *uc, char *note)
{
#ifdef DEBUGMEM
	printf("+---------------------%s------------------+\n",
#ifdef __ve__
	       " VE "
#else
	       " VH "
#endif
		);
	printf("|      %s\n", note);
	for (int b = 0; b < 2; b++)
		printf("| mem[%d] begin=%8u end=%8u %s |\n",
		       b, uc->mem[b].begin, uc->mem[b].end,
		       uc->active == &uc->mem[b] ? "active" : "      ");
	printf("+-------------------------------------------+\n");
#endif
}

// return the non-active free_block pointer
static inline free_block_t *_inactive_free_block(urpc_comm_t *uc)
{
	if (uc->active == &uc->mem[0])
		return &uc->mem[1];
	return &uc->mem[0];
}

static inline uint32_t _free_block_size(free_block_t *b)
{
	return b->end - b->begin;
}

static inline void _fillup_last_and_switch(urpc_comm_t *uc, int slot)
{
	// we were called because there is not enough space.
	// if the we're at the end of the buffer, assign the rest to the
	// last sent request.
	mlist_t *ml = &uc->mlist[slot];
	if (ml->b.len == 0)
		ml->b.offs = uc->active->begin;
	ml->b.len = uc->active->end - ml->b.offs;
	uc->active->begin = uc->active->end = 0;
	uc->active = _inactive_free_block(uc);
	_report_free(uc, "assign tail and switch");
}

static inline void _rebuild_free_blocks(urpc_comm_t *uc, uint64_t last_req, int wanted)
{
	int i, start, mid, slot;
	int on_mb = 0;	// search on mailbox or on mlist
	urpc_mb_t mb;
	mlist_t *ml;
	int abeg, aend, alen, obeg = uc->data_buff_end + 1;
	int sw = 0;	// switch roles of free blocks after allocated region goes through 0
	int count = 0;

#if defined(__ve__) && !defined(SYNCDMA)
	start = uc->dhq.in;
	mid = uc->dhq.out;
#else
	start = mid = REQ2SLOT(last_req);
#endif
	// initialize free block lists to max
	uc->mem[0].begin = uc->mem[1].begin = 0;
	uc->mem[0].end = uc->mem[1].end = uc->data_buff_end;

	// loop down from start to mid
	// in this area the request wasn't submitted, yet (except for the last one)
	slot = start;
	for (int i = 0; i < URPC_LEN_MB; i++) {
		if (slot < 0)
			slot += URPC_LEN_MB;
		if (slot == mid)
			on_mb = 1;
		if (on_mb) {
			mb.u64 = TQ_READ64(uc->tq->mb[slot].u64);
			//TQ_FENCE_L(); TQ_FENCE_S();
			if (mb.c.cmd == URPC_CMD_NONE) // stop search here
				break;
			abeg = mb.c.offs;
			alen = mb.c.len;
		} else {
			// check mlist of current entry
			ml = &uc->mlist[slot];
			abeg = ml->b.offs;
			alen = ml->b.len;
		}
		aend = abeg + alen;
		if (alen) {
			// switch meaning of free blocks when going through zero
			if (abeg > obeg)
				sw = 1;
			if (uc->mem[(0+sw)%2].begin < aend)
				uc->mem[(0+sw)%2].begin = aend;
			if (uc->mem[(1+sw)%2].end > abeg)
				uc->mem[(1+sw)%2].end = abeg;
			obeg = abeg;	// remember last allocation begin
		}
		--slot;
	} 
	uc->active = &uc->mem[0];
#ifdef DEBUGMEM
	_report_free(uc, "GC: after rebuild");
#endif
	//
	// do we have enough space now?
	//
	if (_free_block_size(uc->active) >= wanted)
		return;
	//
	// if we have two blocks (sw == 0 case) then merge upper block
	// to last request and activate the other one.
	//
	if (!sw)
		_fillup_last_and_switch(uc, start);
	_report_free(uc, "GC: after fillup_last_and_switch");
}

/*
  Free payload blocks of finished requests and adjust free block pointers
 */
static uint32_t _gc_buffer(urpc_comm_t *uc, int wanted)
{
	uint64_t last_req = TQ_READ64(uc->tq->last_put_req);
	//TQ_FENCE_L();
#ifdef DEBUGMEM
	_report_free(uc, "GC: starting");
#endif
	//
	// rebuild free_blocks by scanning through planned and active requests
	//
	_rebuild_free_blocks(uc, last_req, wanted);
gc_out:
	return _free_block_size(uc->active);
}
	
/*
  Allocate a payload buffer.

  Returns 0 if allocation failed, otherwise a urpc_mb_t with empty command field
  but filled offs and len fields.
 */
uint64_t alloc_payload(urpc_comm_t *uc, uint32_t size)
{
	if (size > uc->data_buff_end) {
		eprintf("ERROR: data size(%d) exceeds DATA_BUFF_END(%d)\n",size, uc->data_buff_end);
		return 0;
	}
	urpc_mb_t res;
	uint32_t asize = ALIGN8B(size);
        long ts = get_time_us();

	res.u64 = 0;

#ifdef DEBUGMEM
	char msg[40];
	sprintf(msg, "allocate request size=%d", size);
	_report_free(uc, msg);
#endif
	while (uc->active->end - uc->active->begin < asize) {
		uint32_t new_free = _gc_buffer(uc, asize);
		if (new_free < asize) {
			// TODO: delay, count, timeout
#ifdef __ve__
			if (timediff_us(ts) > URPC_ALLOC_TIMEOUT_US) {
				eprintf("alloc_payload timed out!\n");
				return 0;
			}
#else
			return 0;
#endif
		} else {
			break;
                }
	}
	if (uc->active->begin + asize > uc->active->end) {
		printf("alloc: begin=%u end=%u asize=%u\n",
		       uc->active->begin, uc->active->end, asize);
		return 0;
	}
		
	res.c.offs = uc->active->begin;
	uc->active->begin += ALIGN8B(size);
	res.c.len = size;
#ifdef DEBUGMEM
	sprintf(msg, "allocate done (size=%d)", size);
	_report_free(uc, msg);
#endif

	return res.u64;
}

