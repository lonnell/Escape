/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
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

#include <esc/common.h>
#include <ipc/proto/device.h>
#include <ipc/device.h>
#include <memory>
#include <vthrow.h>

namespace ipc {

class FileDevice : public Device {
public:
	explicit FileDevice(const char *path,mode_t mode)
		: Device(path,mode,DEV_TYPE_FILE,DEV_READ | DEV_WRITE) {
		set(MSG_DEV_READ,std::make_memfun(this,&FileDevice::read));
		set(MSG_DEV_WRITE,std::make_memfun(this,&FileDevice::write));
	}

	void read(IPCStream &is) {
		DevRead::Request r;
		is >> r;
		if(r.offset + (off_t)r.count < r.offset)
			VTHROWE("Invalid offset/count (" << r.offset << "," << r.count << ")",-EINVAL);

		std::string content = handleRead();
		ssize_t res = 0;
		if(r.offset >= content.length())
			res = 0;
		else if(r.offset + r.count > content.length())
			res = content.length() - r.offset;
		else
			res = r.count;

		is << DevRead::Response(res) << Send(MSG_DEV_READ_RESP);
		if(res)
			is << SendData(MSG_DEV_READ_RESP,content.c_str(),res);
	}

	void write(IPCStream &is) {
		DevWrite::Request r;
		is >> r;

		std::unique_ptr<uint8_t[]> data(new uint8_t[r.count]);
		is >> ReceiveData(data.get(),r.count);
		ssize_t res = handleWrite(r.offset,data.get(),r.count);

		is << DevWrite::Response(res) << Send(MSG_DEV_WRITE_RESP);
	}

private:
	virtual std::string handleRead() = 0;
	virtual ssize_t handleWrite(size_t,const void *,size_t) {
		return -ENOTSUP;
	}
};

}
