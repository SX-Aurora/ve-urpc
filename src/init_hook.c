#include "urpc_common.h"

handler_init_hook_t urpc_handler_init_hook = NULL;

/*
  Register VH side handler init hook.

  Returns 0 on success, -EEXIST if hook is in use.
*/
void urpc_set_handler_init_hook(void (*func)(urpc_peer_t *up))
{
	urpc_handler_init_hook = func;
}

/*
  Return pointer to urdm_peer init hook function, if set.
 */
handler_init_hook_t urpc_get_handler_init_hook(void)
{
	return urpc_handler_init_hook;
}

