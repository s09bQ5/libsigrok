/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Martin Ling <martin-sigrok@earth.li>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "libsigrok.h"
#include "libsigrok-internal.h"

SR_PRIV int sr_channel_open(struct sr_channel *channel, unsigned int flags)
{
	if (!channel || !channel->open)
		return SR_ERR;
    return channel->open(channel->priv, flags);
}

SR_PRIV int sr_channel_source_add(struct sr_channel *channel, int events, int timeout,
			sr_receive_data_callback_t cb, void *cb_data)
{
	if (!channel || !channel->source_add)
		return SR_ERR;
	return channel->source_add(channel->priv, events, timeout, cb, cb_data);
}

SR_PRIV int sr_channel_source_remove(struct sr_channel *channel)
{
	if (!channel || !channel->source_remove)
		return SR_ERR;
	return channel->source_remove(channel->priv);
}

SR_PRIV int sr_channel_blocking_read(struct sr_channel *channel,
		void *buf, size_t count, unsigned int timeout)
{
	if (!channel || !channel->blocking_read)
		return SR_ERR;
    return channel->blocking_read(channel->priv, buf, count, timeout);
}

SR_PRIV int sr_channel_nonblocking_read(struct sr_channel *channel,
		void *buf, size_t count)
{
	if (!channel || !channel->nonblocking_read)
		return SR_ERR;
    return channel->nonblocking_read(channel->priv, buf, count);
}

SR_PRIV int sr_channel_blocking_write(struct sr_channel *channel,
		const void *buf, size_t count, unsigned int timeout)
{
	if (!channel || !channel->blocking_write)
		return SR_ERR;
    return channel->blocking_write(channel->priv, buf, count, timeout);
}

SR_PRIV int sr_channel_nonblocking_write(struct sr_channel *channel,
		const void *buf, size_t count)
{
	if (!channel || !channel->nonblocking_write)
		return SR_ERR;
    return channel->nonblocking_write(channel->priv, buf, count);
}

SR_PRIV int sr_channel_close(struct sr_channel *channel)
{
	if (!channel || !channel->close)
		return SR_ERR;
    return channel->close(channel->priv);
}

SR_PRIV void sr_channel_free(struct sr_channel *channel)
{
	if (!channel)
		return;
	if (channel->free)
	    channel->free(channel->priv);
	g_free(channel);
}
