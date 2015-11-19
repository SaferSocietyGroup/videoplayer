#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAGIC 0xaabbaacc
#define NO_SEQ_NUM 0

enum CommandType
{
	CTQuit             = 0,
	CTPlay             = 1,
	CTPause            = 2, 
	CTStop             = 3,
	CTSeek             = 4,
	CTLoad             = 5,
	CTUnload           = 6,
	CTLfsConnect       = 7,
	CTLfsDisconnect    = 8,
	CTUpdateOutputSize = 9,
	CTPositionUpdate   = 10,
	CTDuration         = 11,
	CTEof              = 12,
	CTLogMessage       = 13,
	CTForceRedraw      = 14,
	CTSetPlaybackSpeed = 15,
	CTSetVolume        = 16,
	CTSetMute          = 17,
	CTSetQvMute        = 18,

	CTCmdCount
};

enum CommandFlags {
	CFResponse = 1,
};

enum LoadType
{
	LTFile,
	LTLfs
};

enum ArgumentType
{
	ATStr, ATInt32, ATFloat, ATDouble
};

struct CommandSpec
{
	std::vector<ArgumentType> requestArgTypes;
	std::vector<ArgumentType> responseArgTypes;
	bool hasReponse;
};

const std::vector<CommandSpec> CommandSpecs = {
	// command (request argument list) -> (response argument list)
	// a lot of commands respond with an empty response to indicate that the command finished

	// quit
	{ {}, {}, false },
	
	// play -> ()
	{ {}, {}, true },
	
	// pause -> ()
	{ {}, {}, true },
	
	// stop -> ()
	{ {}, {}, true },

	// seek (seconds) -> ()
	{ {ATFloat}, {}, true },

	// load (loadtype, path) -> success?
	{ {ATInt32, ATStr}, {ATInt32}, true },

	// unload -> ()
	{ {}, {}, true },

	// lfs connect (pipename) -> (success)?
	{ {ATStr}, {ATInt32}, true },

	// lfs disconnect
	{ {}, {}, false },

	// update output size (w, h)
	{ {ATInt32, ATInt32}, {}, false },

	// position update
	{ {ATFloat}, {}, false },

	// duration(seconds)
	{ {ATFloat}, {}, false },

	// eof
	{ {}, {}, false },

	// log message (verbosity, lineNumber, file, message)
	{ {ATInt32, ATInt32, ATStr, ATStr}, {}, false },

	// force redraw
	{ {}, {}, false },

	// set playback speed (speed)
	{ {ATFloat}, {}, false },
	
	// set volume (volume)
	{ {ATFloat}, {}, false },

	// set mute (1/0)
	{ {ATInt32}, {}, false },

	// set quickviewmute (1/0)
	{ {ATInt32}, {}, false },
};

struct Argument
{
	ArgumentType type;

	int32_t i;
	std::wstring str;
	float f;
	double d;
};

struct Command
{
	CommandType type;
	std::vector<Argument> args;
	uint32_t seqNum;
	uint32_t flags;
};

#endif
