#ifndef PACKET_H
#define PACKET_H

#include "avlibs.h"

typedef std::shared_ptr<class Packet> PacketPtr;

class Packet
{
	public:
	AVPacket avPacket;

	Packet()
	{
		memset(&avPacket, 0, sizeof(AVPacket));
	}

	~Packet()
	{
		// note: does nothing if packet.buf is NULL, so it's ok if avPacket is empty
		av_free_packet(&avPacket);
	}

	static PacketPtr Create()
	{
		return std::make_shared<Packet>();
	}
};

#endif
