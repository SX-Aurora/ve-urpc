#include "urpc_common.h"

#define MAX_INIT_HOOKS 10
static int num_hooks = 0;

typedef void (*handler_init_hook_t)(urpc_peer_t *);
static handler_init_hook_t init_hook[MAX_INIT_HOOKS] = { NULL };

/*
  Register VH side handler init hook.

  Returns 0 on success, -EEXIST if hook is in use.
*/
void urpc_set_handler_init_hook(void (*func)(urpc_peer_t *up))
{
	if (num_hooks < MAX_INIT_HOOKS)
		init_hook[num_hooks++] = func;
	else {
		eprintf("Maximum number of handler init hooks reached! Ignoring set request.\n");
	}
}

/*
  Run init hook functions.
 */
void urpc_run_handler_init_hooks(urpc_peer_t *up)
{
	for (int i = 0; i < num_hooks; i++)
		init_hook[i](up);
}

