#include "Decoder.h"
#include "amrnb/interf_dec.h"

using AMR_HEADER_T = std::array<unsigned char, 6>;
constexpr AMR_HEADER_T AMR_HEADER = {35, 33, 65, 77, 82, 10 }; // '#!AMR'#10

Buffer::Buffer(int size) 
{
	this->used = 0;
	this->size = size;
	this->data = (BYTE*)malloc(size);
}

Buffer::~Buffer()
{
	free(data);
}

int Buffer::extract(BYTE* target, int maxTarget)
{
	int copy = maxTarget > used ? used : maxTarget;
	memcpy(target, data, copy);
	if (copy < used)
		memcpy(&data[0], &data[copy], used - copy);
	used -= copy;
	return copy;
}

/* Decoder */

Decoder::Decoder(IAIMPStream* stream, void* decoder)
{
	this->decoder = decoder;
	this->stream = stream;
	this->stream->AddRef();
	this->bufferAMR = new Buffer(FrameMaxSize);
	this->bufferPCM = new Buffer(BytesPerFrame);
	buildSeekTable();
}

Decoder::~Decoder()
{
	Decoder_Interface_exit(decoder);
	stream->Release();
	delete bufferAMR;
	delete bufferPCM;
}

void Decoder::buildSeekTable()
{
	seekTable.clear();
	seekTable.reserve(size_t(stream->GetSize()) / 16); // приблизительное количество

	unsigned char frameId;
	// stream уже смещен к секции с данными - на размер заголовка (см. tryCreate)
	while (stream->Read(&frameId, sizeof(frameId)) == sizeof(frameId))
	{
		seekTable.push_back(stream->GetPosition() - sizeof(frameId));
		stream->Seek(FrameSizes[(frameId >> 3) & 0x0F], AIMP_STREAM_SEEKMODE_FROM_CURRENT);
	}
	seekTable.push_back(stream->GetSize());
}

BOOL WINAPI Decoder::GetFileInfo(IAIMPFileInfo* FileInfo)
{
	return FALSE;
}

BOOL WINAPI Decoder::GetStreamInfo(INT32* SampleRate, INT32* Channels, INT32* SampleFormat)
{
	(*Channels) = Decoder::Channels;
	(*SampleRate) = Decoder::SampleRate;
	(*SampleFormat) = AIMP_DECODER_SAMPLEFORMAT_16BIT;
	return TRUE;
}

BOOL WINAPI Decoder::IsSeekable()
{
	return TRUE;
}

BOOL WINAPI Decoder::IsRealTimeStream()
{
	return FALSE;
}

BOOL Decoder::isOurRIID(REFIID riid)
{
	return EqualGUID(riid, IID_IAIMPAudioDecoder);
}

INT64 WINAPI Decoder::GetAvailableData()
{
	return GetSize() - GetPosition();
}

INT64 WINAPI Decoder::GetSize()
{
	return seekTable.size() * BytesPerFrame;
}

INT64 WINAPI Decoder::GetPosition()
{
	INT64 res = 0;
	INT64 pos = stream->GetPosition();
	for (size_t i = 0; i < seekTable.size(); i++) 
	{
		if (seekTable.at(i) <= pos)
			res = i;
		else
			break;
	}
	return res * BytesPerFrame;
}

BOOL WINAPI Decoder::SetPosition(const INT64 Value)
{
	size_t frame = (size_t)(Value / INT64(BytesPerFrame));
	if (frame >= 0 && frame < seekTable.size())
	{
		stream->Seek(seekTable.at(frame), AIMP_STREAM_SEEKMODE_FROM_BEGINNING);
		bufferAMR->used = 0;
		bufferPCM->used = 0;
		return TRUE;
	}
	return FALSE;
}

INT32 WINAPI Decoder::Read(void* target, INT32 count)
{
	INT32 requested = count;
	BYTE* buffer = (BYTE*)target;
	while (count > 0)
	{
		if (bufferPCM->used == 0) 
		{
			if (!readFrameAndDecodeIt())
				break;
		}
		int bytes = bufferPCM->extract(buffer, count);
		buffer += bytes;
		count -= bytes;
	}
	return requested - count;
}

BOOL Decoder::readFrameAndDecodeIt()
{
	try 
	{
		stream->Read(&bufferAMR->data[0], 1); // frame ID
		int frameSize = FrameSizes[(bufferAMR->data[0] >> 3) & 0x0F];
		bufferAMR->used = frameSize + 1;		
		stream->Read(&bufferAMR->data[1], frameSize); // frame data
		Decoder_Interface_Decode(decoder, bufferAMR->data, (short*)bufferPCM->data, 0);
		bufferPCM->used = bufferPCM->size;
		return TRUE;
	}
	catch (const std::exception&) 
	{
		return FALSE;
	}
}

Decoder* Decoder::tryCreate(IAIMPStream* stream)
{
	AMR_HEADER_T header = {};
	if (stream->Read(&header, sizeof(header)) == sizeof(header))
	{
		if (std::memcmp(header.data(), AMR_HEADER.data(), sizeof(header)) == 0)
		{
			void* handle = Decoder_Interface_init();
			if (handle != nullptr)
				return new Decoder(stream, handle);
		}
	}
	return nullptr;
}