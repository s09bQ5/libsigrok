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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct usbtmc_data {
	char *device;
	int fd;
};

SR_PRIV int usbtmc_open(void *priv, unsigned int flags)
{
	struct usbtmc_data *usbtmc = priv;
	int open_flags = O_NONBLOCK;

	if (flags & (CHANNEL_READ | CHANNEL_WRITE))
		open_flags = O_RDWR;
	else if (flags & CHANNEL_READ)
		open_flags = O_RDONLY;
	else if (flags & CHANNEL_WRITE)
		open_flags = O_WRONLY;

	if ((usbtmc->fd = open(usbtmc->device, open_flags)) < 0)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int usbtmc_source_add(void *priv, int events, int timeout,
			sr_receive_data_callback_t cb, void *cb_data)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_source_add(usbtmc->fd, events, timeout, cb, cb_data);
}

SR_PRIV int usbtmc_source_remove(void *priv)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_source_remove(usbtmc->fd);
}

SR_PRIV int usbtmc_blocking_read(void *priv,
		void *buf, size_t count, unsigned int timeout)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_fd_read_blocking(usbtmc->fd, buf, count, timeout);
}

SR_PRIV int usbtmc_nonblocking_read(void *priv,
		void *buf, size_t count)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_fd_read_nonblocking(usbtmc->fd, buf, count);
}

SR_PRIV int usbtmc_blocking_write(void *priv,
		const void *buf, size_t count, unsigned int timeout)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_fd_write_blocking(usbtmc->fd, buf, count, timeout);
}

SR_PRIV int usbtmc_nonblocking_write(void *priv,
		const void *buf, size_t count)
{
	struct usbtmc_data *usbtmc = priv;
	return sr_fd_write_nonblocking(usbtmc->fd, buf, count);
}

SR_PRIV int usbtmc_close(void *priv)
{
	struct usbtmc_data *usbtmc = priv;
	int result;
	result = close(usbtmc->fd);
	usbtmc->fd = -1;
	return result;
}

SR_PRIV void usbtmc_free(void *priv)
{
	struct usbtmc_data *usbtmc = priv;
	g_free(usbtmc->device);
	g_free(usbtmc);
}

SR_PRIV struct sr_channel *usbtmc_channel_new(const char *device)
{
	struct sr_channel *channel;
	struct usbtmc_data *usbtmc;

	channel = g_try_malloc(sizeof(struct sr_channel));
	channel->open = usbtmc_open;
	channel->source_add = usbtmc_source_add;
	channel->source_remove = usbtmc_source_remove;
	channel->blocking_read = usbtmc_blocking_read;
	channel->nonblocking_read = usbtmc_nonblocking_read;
	channel->blocking_write = usbtmc_blocking_write;
	channel->nonblocking_write = usbtmc_nonblocking_write;
	channel->close = usbtmc_close;
	channel->free = usbtmc_free;
	channel->priv = usbtmc = g_try_malloc(sizeof(struct usbtmc_data));
	usbtmc->device = g_strdup(device);
	usbtmc->fd = -1;

	return channel;
}
