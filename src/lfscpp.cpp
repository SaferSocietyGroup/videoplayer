#include <lfsc.h>

#include "lfscpp.h"
#include "tools.h"

class CLfscpp : public Lfscpp
{
	public:
	lfsc_ctx* ctx;

	CLfscpp()
	{
		ctx = lfsc_ctx_create(); 
	}

	~CLfscpp()
	{
		lfsc_ctx_destroy(ctx);
	}

	void Connect(const std::wstring name, int msTimeout)
	{
		lfsc_ctx_connect(ctx, name.c_str(), msTimeout);
	}

	void Disconnect()
	{
		lfsc_ctx_disconnect(ctx);
	}

	IpcStreamPtr Open(const std::wstring name)
	{
		lfsc_file* f;
		lfsc_status s = lfsc_ctx_fopen(ctx, &f, name.c_str());

		if(s != LFSC_SOK)
			throw StreamEx(/*Str("could not open file: " << Tools::WstrToStr(name) << ", status: " << (int)s)*/ "could not open file");

		IpcStreamPtr stream = IpcStream::Create();
		stream->Open(name, f);
		return stream;
	}
};

LfscppPtr Lfscpp::Create()
{
	return std::make_shared<CLfscpp>();
}
