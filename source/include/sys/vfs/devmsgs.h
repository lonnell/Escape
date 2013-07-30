/**
 * $Id$
 * Copyright (C) 2008 - 2011 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <sys/common.h>
#include <sys/vfs/node.h>

/**
 * Sends the open-command to the device and waits for its response
 *
 * @param pid the process-id
 * @param file the file for the device
 * @param node the VFS-node of the device
 * @param flags the open-flags
 * @return 0 if successfull
 */
ssize_t vfs_devmsgs_open(pid_t pid,OpenFile *file,sVFSNode *node,uint flags);

/**
 * Reads <count> bytes at <offset> into <buffer> from the given device. Note that not all
 * devices will use the offset. Some may simply ignore it.
 *
 * @param pid the process-id
 * @param file the file for the device
 * @param node the VFS-node of the device
 * @param buffer the buffer where to copy the data
 * @param offset the offset from which to read
 * @param count the number of bytes to read
 * @return the number of read bytes or a negative error-code
 */
ssize_t vfs_devmsgs_read(pid_t pid,OpenFile *file,sVFSNode *node,void *buffer,off_t offset,size_t count);

/**
 * Writes <count> bytes to <offset> from <buffer> to the given device. Note that not all
 * devices will use the offset. Some may simply ignore it.
 *
 * @param pid the process-id
 * @param file the file for the device
 * @param node the VFS-node of the device
 * @param buffer the buffer from which copy the data
 * @param offset the offset to write to
 * @param count the number of bytes to write
 * @return the number of written bytes or a negative error-code
 */
ssize_t vfs_devmsgs_write(pid_t pid,OpenFile *file,sVFSNode *node,const void *buffer,off_t offset,
		size_t count);

/**
 * Sends the close-command to the given device
 *
 * @param pid the process-id
 * @param file the file for the device
 * @param node the VFS-node of the device
 */
void vfs_devmsgs_close(pid_t pid,OpenFile *file,sVFSNode *node);
