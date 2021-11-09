#include "urpc_common.h"

/**
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *
 * Handler initialization hooks.
 * 
 * Copyright (c) 2020 Erich Focht
 */

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

