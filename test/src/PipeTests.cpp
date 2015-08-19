#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <thread>

#include "PipeTests.h"
#include "Pipe.h"
#include "Flog.h"

class CPipeTests : public PipeTests
{
	public:
	void RegisterTests(std::vector<Test>& testSet)
	{
		testSet.push_back({"Pipe", "Create", [&]{Create();} });
		testSet.push_back({"Pipe", "CreateAndOpen", [&]{CreateAndOpen();} });
		testSet.push_back({"Pipe", "ReadWriteInt32", [&]{ReadWriteInt32();} });
		testSet.push_back({"Pipe", "ReadWriteUInt32", [&]{ReadWriteUInt32();} });
		testSet.push_back({"Pipe", "ReadWriteFloat", [&]{ReadWriteFloat();} });
		testSet.push_back({"Pipe", "ReadWriteDouble", [&]{ReadWriteDouble();} });
		testSet.push_back({"Pipe", "EncodeLEB128", [&]{EncodeLEB128();} });
		testSet.push_back({"Pipe", "DecodeLEB128", [&]{DecodeLEB128();} });
		testSet.push_back({"Pipe", "EncodeDecodeLEB128", [&]{EncodeDecodeLEB128();} });
		testSet.push_back({"Pipe", "ReadWriteLEB128", [&]{ReadWriteLEB128();} });
		testSet.push_back({"Pipe", "ReadWriteString", [&]{ReadWriteString();} });
		testSet.push_back({"Pipe", "ReadBeforeWrite", [&]{ReadBeforeWrite();} });
	}

	void Create()
	{
		PipePtr p = Pipe::Create(); 
		p->CreatePipe(L"hello");
	}

	void CreateAndOpen()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");
	}

	void ReadWriteInt32()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<int32_t> ints = {0, 0x123, 6, 888, -110000, 133188181, INT_MAX, INT_MIN};

		for(int32_t i : ints){
			host->WriteInt32(i);
			int32_t ri = client->ReadInt32();
			TAssertEquals(ri, i);
		}

		for(int32_t i : ints){
			client->WriteInt32(i);
			int32_t ri = host->ReadInt32();
			TAssertEquals(ri, i);
		}
	}

	void ReadWriteUInt32()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<uint32_t> ints = {0, 0x123, 6, 888, 3281931, 133188181, UINT_MAX};

		for(uint32_t i : ints){
			host->WriteInt32(i);
			uint32_t ri = client->ReadInt32();
			TAssertEquals(ri, i);
		}

		for(uint32_t i : ints){
			client->WriteInt32(i);
			uint32_t ri = host->ReadInt32();
			TAssertEquals(ri, i);
		}
	}
	
	void ReadWriteFloat()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<float> nums = {0.001f, -1.f, .123f, 12831238971.1f, -333.1f};

		for(auto i : nums){
			host->WriteFloat(i);
			auto ri = client->ReadFloat();
			TAssertEquals(ri, i);
		}

		for(auto i : nums){
			client->WriteFloat(i);
			auto ri = host->ReadFloat();
			TAssertEquals(ri, i);
		}
	}
	
	void ReadWriteDouble()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<double> nums = {0.001f, -1.f, .123f, 12831238971.1f, -333.1f};

		for(auto i : nums){
			host->WriteFloat(i);
			auto ri = client->ReadFloat();
			TAssertEquals(ri, i);
		}

		for(auto i : nums){
			client->WriteFloat(i);
			auto ri = host->ReadFloat();
			TAssertEquals(ri, i);
		}
	}
	
	void DecodeLEB128()
	{
		PipePtr p = Pipe::Create();

		std::vector<uint64_t> out = {
			624485, 0
		};

		std::vector<std::vector<uint8_t>> in = {
			{ 0xE5, 0x8E, 0x26 }, {0}
		};

		for(int i = 0; i < (int)in.size(); i++){
			uint64_t r = p->DecodeLEB128((char*)in[i].data(), in[i].size());
			TAssertEquals(r, out[i]);
		}
	}

	void EncodeLEB128()
	{
		char buffer[32];
		PipePtr p = Pipe::Create();

		std::vector<uint64_t> in = {
			624485, 0
		};

		std::vector<std::vector<uint8_t>> out = {
			{ 0xE5, 0x8E, 0x26 }, {0}
		};

		for(int i = 0; i < (int)in.size(); i++){
			int len = p->EncodeLEB128(buffer, in[i]);
			TAssertEquals(len, (int)out[i].size());

			for(int j = 0; j < len; j++){
				TAssertEquals((unsigned)(uint8_t)buffer[j], (unsigned)out[i][j]);
			}
		}
	}
	
	void EncodeDecodeLEB128()
	{
		char buffer[32];
		PipePtr p = Pipe::Create();

		std::vector<uint64_t> nums = {
			624485, 0, 0x1231, 1, 2, 3, INT_MAX, UINT_MAX, std::numeric_limits<uint64_t>::max()
		};

		for(uint64_t v : nums){
			p->EncodeLEB128(buffer, v);
			uint64_t r = p->DecodeLEB128(buffer, sizeof(buffer));
			TAssertEquals(v, r);
		}
	}
	
	void ReadWriteLEB128()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<uint64_t> nums = {
			624485, 0, 0x1231, 1, 2, 3, INT_MAX, UINT_MAX, std::numeric_limits<uint64_t>::max()
		};

		for(uint64_t v : nums){
			host->WriteLEB128(v);
			uint64_t r = client->ReadLEB128();
			TAssertEquals(v, r);
		}
	}
	
	void ReadBeforeWrite()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");
		
		std::thread wt([&]{
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
			host->WriteInt32(32);
		});

		int r = client->ReadInt32();

		TAssertEquals(32, r);

		wt.join();
	}
	
	void ReadWriteString()
	{
		PipePtr host, client;

		host = Pipe::Create(); 
		host->CreatePipe(L"hello");

		client = Pipe::Create();
		client->Open(L"hello");

		std::vector<std::wstring> strs = {
			L"hello", L"a", L"", L"asdasd", L"MY LIFE"
		};

		std::wstring big;
		big.reserve(10 * 1024 * 1024);

		std::wstring phrase = L"aksdpoq6485123648512vvdaavf";

		for(int i = 0; i < 10 * 1024 * 1024; i++)
			big[i] = phrase[i  % phrase.size()];

		strs.push_back(big);


		std::thread wt([&]{
			for(std::wstring s : strs){
				host->WriteString(s);
			}
		});

		std::thread rt([&]{
			for(std::wstring s : strs){
				std::wstring r;
				client->ReadString(r);
				TAssert(s == r, "string mismatch, got: '" << Tools::WstrToStr(r) << "' expected: '" << Tools::WstrToStr(s) << "'");
			}
		});

		rt.join();
		wt.join();
	}
};

PipeTestsPtr PipeTests::Create()
{
	return std::make_shared<CPipeTests>();
}
