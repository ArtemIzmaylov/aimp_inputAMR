#include "Decoder.h"
#include "amrnb/interf_dec.h"
#include "amrwb/dec_if.h"

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

Decoder::Decoder(IAIMPStream* stream, int maxFrameSize, int samplerate, const t_frame_sizes frameSizes)
{
	this->stream = stream;
	this->stream->AddRef();
	this->sampleRate = samplerate;
	this->frameSizes = frameSizes;
	this->bytesPerFrame = (samplerate * Channels * (BitDepth / 8) * FrameDuration) / 1000;
	this->bufferAMR = new Buffer(maxFrameSize);
	this->bufferPCM = new Buffer(bytesPerFrame);
	buildSeekTable();
}

Decoder::~Decoder()
{
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
		int frameSize = getFrameSize(frameId);
		if (frameSize < 0) break;
		stream->Seek(frameSize, AIMP_STREAM_SEEKMODE_FROM_CURRENT);
	}
	seekTable.push_back(stream->GetSize());
}

BOOL WINAPI Decoder::GetFileInfo(IAIMPFileInfo* FileInfo)
{
	return false;
}

BOOL WINAPI Decoder::GetStreamInfo(INT32* SampleRate, INT32* Channels, INT32* SampleFormat)
{
	(*Channels) = Decoder::Channels;
	(*SampleRate) = this->sampleRate;
	(*SampleFormat) = AIMP_DECODER_SAMPLEFORMAT_16BIT;
	return true;
}

BOOL WINAPI Decoder::IsSeekable()
{
	return true;
}

BOOL WINAPI Decoder::IsRealTimeStream()
{
	return false;
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
	return seekTable.size() * bytesPerFrame;
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
	return res * bytesPerFrame;
}

BOOL WINAPI Decoder::SetPosition(const INT64 Value)
{
	size_t frame = (size_t)(Value / INT64(bytesPerFrame));
	if (frame >= 0 && frame < seekTable.size())
	{
		stream->Seek(seekTable.at(frame), AIMP_STREAM_SEEKMODE_FROM_BEGINNING);
		bufferAMR->used = 0;
		bufferPCM->used = 0;
		return true;
	}
	return false;
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

int Decoder::getFrameSize(BYTE frameId)
{
	return frameSizes.sizes[(frameId >> 3) & 0x0F];
}

BOOL Decoder::readFrameAndDecodeIt()
{
	try 
	{
		stream->Read(&bufferAMR->data[0], 1); // frame ID
		int frameSize = getFrameSize(bufferAMR->data[0]);
		if (frameSize < 0) return false;
		bufferAMR->used = frameSize + 1;		
		stream->Read(&bufferAMR->data[1], frameSize); // frame data
		decode();
		bufferPCM->used = bufferPCM->size;
		return true;
	}
	catch (const std::exception&) 
	{
		return false;
	}
}

Decoder* Decoder::tryCreate(IAIMPStream* stream)
{
	unsigned char header[6] = {};
	if (stream->Read(&header, 6) == 6)
	{
		if (header[0] != 35) // #
			return nullptr;
		if (header[1] != 33) // !
			return nullptr;
		if (header[2] != 65) // A
			return nullptr;
		if (header[3] != 77) // R
			return nullptr;
		if (header[4] != 82) // M
			return nullptr;
		if (header[5] == 10) // Narrow-Band
		{
			void* handle = Decoder_Interface_init();
			if (handle != nullptr)
				return new DecoderNarrowBand(stream, handle);
		}
		else 
			if (header[5] == 45) // Wide-Band possible?
			{
				if (stream->Read(&header, 3) == 3)
				{
					if (header[0] != 87) // W
						return nullptr;
					if (header[1] != 66) // B
						return nullptr;
					if (header[2] == 10) // Wide-Band!
					{
						void* handle = D_IF_init();
						if (handle != nullptr)
							return new DecoderWideBand(stream, handle);
					}
				}
			}
	}
	return nullptr;
}

/* DecoderNarrowBand */

DecoderNarrowBand::DecoderNarrowBand(IAIMPStream* stream, void* decoder):
	Decoder(stream, 32, 8000, { 12, 13, 15, 17, 19, 20, 26, 31, 5, 0, 0, 0, 0, 0, 0, 0 })
{
	this->decoder = decoder;
};

DecoderNarrowBand::~DecoderNarrowBand()
{
	Decoder_Interface_exit(decoder);
}

void DecoderNarrowBand::decode()
{
	Decoder_Interface_Decode(decoder, bufferAMR->data, (short*)bufferPCM->data, 0);
}

/* DecoderWideBand */

DecoderWideBand::DecoderWideBand(IAIMPStream* stream, void* decoder): 
	Decoder(stream, 64, 16000, { 17, 23, 32, 36, 40, 46, 50, 58, 60, 5, -1, -1, -1, -1, -1, 0 })
{
	this->decoder = decoder;
};

DecoderWideBand::~DecoderWideBand()
{
	D_IF_exit(decoder);
}

void DecoderWideBand::decode()
{
	D_IF_decode(decoder, bufferAMR->data, (short*)bufferPCM->data, 0);
}
