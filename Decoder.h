#pragma once
#include <apiDecoders.h>
#include <apiPlugin.h>
#include <apiTypes.h>
#include <apiWrappers.h>
#include <IUnknownImpl.h>
#include <array>
#include <vector>

typedef struct {
	int sizes[16];
} t_frame_sizes;

/* Buffer */

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

/* Decoder */

class Decoder : public IUnknownImpl<IAIMPAudioDecoder>
{
private:
	const int BitDepth = 16;
	const int Channels = 1;
	const int FrameDuration = 20; // msec
private:
	int bytesPerFrame;
	int sampleRate;
	t_frame_sizes frameSizes;
	std::vector<INT64> seekTable;
	IAIMPStream* stream;

	int getFrameSize(BYTE frameId);
	BOOL readFrameAndDecodeIt();
protected:
	Buffer* bufferAMR;
	Buffer* bufferPCM;

	void buildSeekTable();
	virtual void decode() = 0;
public:
	 Decoder(IAIMPStream* stream, int maxFrameSize, int sampleRate, const t_frame_sizes frameSizes);
	~Decoder();
	static Decoder* tryCreate(IAIMPStream* stream);

	// IAIMPAudioDecoder
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

/* DecoderNarrowBand */

class DecoderNarrowBand : public Decoder
{
protected:
	void* decoder;
	virtual void decode();
public:
	DecoderNarrowBand(IAIMPStream* stream, void* decoder);
	~DecoderNarrowBand();
};

/* DecoderWideBand */

class DecoderWideBand : public Decoder
{
protected:
	void* decoder;
	virtual void decode();
public:
	DecoderWideBand(IAIMPStream* stream, void* decoder);
	~DecoderWideBand();
};
