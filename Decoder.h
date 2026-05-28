#pragma once
#include "framework.h"
#include <apiDecoders.h>
#include <apiPlugin.h>
#include <apiTypes.h>
#include <apiWrappers.h>
#include <IUnknownImpl.h>
#include <array>
#include <vector>

class Buffer
{
public:
	BYTE* data;
	int size; // readonly
	int used;
	 Buffer(int size);
	~Buffer();
	int extract(BYTE* target, int maxTarget);
};

class Decoder : public IUnknownImpl<IAIMPAudioDecoder>
{
private:
	const int BitDepth = 16;
	const int Channels = 1;
	const int SampleRate = 8000;
	const int FrameDuration = 20; // msec
	const int FrameMaxSize = 32;
	const int FrameSizes[16] = {12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0};
	const int BytesPerFrame = (SampleRate * Channels * (BitDepth / 8) * FrameDuration) / 1000;
private:
	void* decoder;
	Buffer* bufferAMR;
	Buffer* bufferPCM;
	std::vector<INT64> seekTable;
	IAIMPStream* stream;

	void buildSeekTable();
	BOOL readFrameAndDecodeIt();
public:
	 Decoder(IAIMPStream* stream, void* decoder);
	~Decoder();
	static Decoder* tryCreate(IAIMPStream* stream);

	BOOL WINAPI GetFileInfo(IAIMPFileInfo* FileInfo);
	BOOL WINAPI GetStreamInfo(INT32* SampleRate, INT32* Channels, INT32* SampleFormat);

	BOOL WINAPI IsSeekable();
	BOOL WINAPI IsRealTimeStream();
	BOOL isOurRIID(REFIID riid);

	INT64 WINAPI GetAvailableData();
	INT64 WINAPI GetSize();
	INT64 WINAPI GetPosition();
	BOOL WINAPI SetPosition(const INT64 Value);

	INT32 WINAPI Read(void* target, INT32 count);
};