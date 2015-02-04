#ifndef LFSCPP_H
#define LFSCPP_H

#include <memory>
#include <string>

#include "ipcstream.h"

typedef std::shared_ptr<class Lfscpp> LfscppPtr;

class Lfscpp
{
	public:
	virtual void Connect(const std::wstring name, int msTimeout) = 0;
	virtual void Disconnect() = 0;
	virtual IpcStreamPtr Open(const std::wstring name) = 0;

	static LfscppPtr Create();
};

#endif
