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
#include <unistd.h>
#include <errno.h>

/* Message logging helpers with subsystem-specific prefix string. */
#define LOG_PREFIX "fd: "
#define sr_log(l, s, args...) sr_log(l, LOG_PREFIX s, ## args)
#define sr_spew(s, args...) sr_spew(LOG_PREFIX s, ## args)
#define sr_dbg(s, args...) sr_dbg(LOG_PREFIX s, ## args)
#define sr_info(s, args...) sr_info(LOG_PREFIX s, ## args)
#define sr_warn(s, args...) sr_warn(LOG_PREFIX s, ## args)
#define sr_err(s, args...) sr_err(LOG_PREFIX s, ## args)

SR_PRIV int sr_fd_read_blocking(int fd, void *buf, size_t count, unsigned int timeout)
{
	size_t bytes_read = 0;
	unsigned char *ptr = (unsigned char *) buf;
	struct timeval start, delta, now, end = {0, 0};
	fd_set fds;
	int result;

	if (fd < 0)
		return SR_ERR;

	if (!buf)
		return SR_ERR;

	if (count == 0)
		return 0;

	if (timeout) {
		gettimeofday(&start, NULL);
		delta.tv_sec = timeout / 1000;
		delta.tv_usec = (timeout % 1000) * 1000;
		timeradd(&start, &delta, &end);
	}

	sr_spew("starting blocking read");

	while (bytes_read < count)
	{
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (timeout) {
			gettimeofday(&now, NULL);
			if (timercmp(&now, &end, >)) {
				sr_spew("timeout expired, read %d bytes", bytes_read);
				return bytes_read;
			}
			timersub(&end, &now, &delta);
		}
		sr_spew("starting select()");
		result = select(fd + 1, &fds, NULL, NULL, timeout ? &delta : NULL);
		sr_spew("select() returned %d", result);
		if (result < 0) {
			if (errno == EINTR) {
				sr_spew("call interrupted");
				continue;
			} else {
				sr_err("select() error: %m");
				return SR_ERR;
			}
		} else if (result == 0) {
			sr_spew("timeout in select, %d bytes read", bytes_read);
			return bytes_read;
		}

		sr_spew("trying to read %d bytes", count - bytes_read);
		result = read(fd, ptr, count - bytes_read);

		if (result < 0) {
			if (errno == EAGAIN) {
				sr_spew("no data");
				continue;
			} else {
				sr_err("read() error: %m");
				return SR_ERR;
			}
		}

		sr_spew("read %d bytes", result);
		bytes_read += result;
		ptr += result;
	}

	sr_spew("complete, %d bytes read", bytes_read);
	return bytes_read;
}

SR_PRIV int sr_fd_read_nonblocking(int fd, void *buf, size_t count)
{
	ssize_t bytes_read;

	if (fd < 0)
		return SR_ERR;

	if (!buf)
		return SR_ERR;

	if ((bytes_read = read(fd, buf, count)) < 0) {
		if (errno == EAGAIN)
			bytes_read = 0;
		else
			return SR_ERR;
	}

	return bytes_read;
}

SR_PRIV int sr_fd_write_blocking(int fd, const void *buf, size_t count, unsigned int timeout)
{
	size_t bytes_written = 0;
	unsigned char *ptr = (unsigned char *) buf;
	struct timeval start, delta, now, end = {0, 0};
	fd_set fds;
	int result;

	if (fd < 0)
		return SR_ERR;

	if (!buf)
		return SR_ERR;

	if (count == 0)
		return 0;

	if (timeout) {
		gettimeofday(&start, NULL);
		delta.tv_sec = timeout / 1000;
		delta.tv_usec = (timeout % 1000) * 1000;
		timeradd(&start, &delta, &end);
	}

	while (bytes_written < count)
	{
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		if (timeout) {
			gettimeofday(&now, NULL);
			if (timercmp(&now, &end, >)) {
				return bytes_written;
			}
			timersub(&end, &now, &delta);
		}
		result = select(fd + 1, NULL, &fds, NULL, timeout ? &delta : NULL);
		if (result < 0) {
			if (errno == EINTR)
				continue;
			else
				return SR_ERR;
		} else if (result == 0) {
			return bytes_written;
		}

		result = write(fd, ptr, count - bytes_written);

		if (result < 0) {
			if (errno == EAGAIN)
				continue;
			else
				return SR_ERR;
		}

		bytes_written += result;
		ptr += result;
	}

	return bytes_written;
}

SR_PRIV int sr_fd_write_nonblocking(int fd, const void *buf, size_t count)
{
	ssize_t written;

	if (fd < 0)
		return SR_ERR;

	if (!buf)
		return SR_ERR;

	if (count == 0)
		return 0;

	written = write(fd, buf, count);

	if (written < 0)
		return SR_ERR;
	else
		return written;
}
