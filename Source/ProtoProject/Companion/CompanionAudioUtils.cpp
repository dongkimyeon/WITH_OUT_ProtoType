// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionAudioUtils.h"

namespace
{
	void AppendUint32(TArray<uint8>& Out, uint32 Value)
	{
		Out.Add((uint8)(Value & 0xFF));
		Out.Add((uint8)((Value >> 8) & 0xFF));
		Out.Add((uint8)((Value >> 16) & 0xFF));
		Out.Add((uint8)((Value >> 24) & 0xFF));
	}

	void AppendUint16(TArray<uint8>& Out, uint16 Value)
	{
		Out.Add((uint8)(Value & 0xFF));
		Out.Add((uint8)((Value >> 8) & 0xFF));
	}

	uint32 ReadUint32(const uint8* Data)
	{
		return (uint32)Data[0] | ((uint32)Data[1] << 8) | ((uint32)Data[2] << 16) | ((uint32)Data[3] << 24);
	}

	uint16 ReadUint16(const uint8* Data)
	{
		return (uint16)(Data[0] | (Data[1] << 8));
	}
}

void CompanionAudioUtils::BuildWavFromFloatPCM(const TArray<float>& InSamples, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWavBytes)
{
	const int32 BitsPerSample = 16;
	const int32 BlockAlign = NumChannels * (BitsPerSample / 8);
	const int32 ByteRate = SampleRate * BlockAlign;
	const uint32 DataSize = InSamples.Num() * (BitsPerSample / 8);

	OutWavBytes.Reset();
	OutWavBytes.Reserve(44 + DataSize);

	// RIFF header
	OutWavBytes.Append({ 'R','I','F','F' });
	AppendUint32(OutWavBytes, 36 + DataSize);
	OutWavBytes.Append({ 'W','A','V','E' });

	// fmt chunk
	OutWavBytes.Append({ 'f','m','t',' ' });
	AppendUint32(OutWavBytes, 16); // PCM fmt chunk size
	AppendUint16(OutWavBytes, 1); // PCM format
	AppendUint16(OutWavBytes, (uint16)NumChannels);
	AppendUint32(OutWavBytes, (uint32)SampleRate);
	AppendUint32(OutWavBytes, (uint32)ByteRate);
	AppendUint16(OutWavBytes, (uint16)BlockAlign);
	AppendUint16(OutWavBytes, (uint16)BitsPerSample);

	// data chunk
	OutWavBytes.Append({ 'd','a','t','a' });
	AppendUint32(OutWavBytes, DataSize);

	for (float Sample : InSamples)
	{
		const float Clamped = FMath::Clamp(Sample, -1.0f, 1.0f);
		const int16 Pcm16 = (int16)(Clamped * 32767.0f);
		AppendUint16(OutWavBytes, (uint16)Pcm16);
	}
}

bool CompanionAudioUtils::ParseWavPCM(const TArray<uint8>& InWavBytes, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample)
{
	if (InWavBytes.Num() < 44)
	{
		return false;
	}

	const uint8* Data = InWavBytes.GetData();
	if (Data[0] != 'R' || Data[1] != 'I' || Data[2] != 'F' || Data[3] != 'F' ||
		Data[8] != 'W' || Data[9] != 'A' || Data[10] != 'V' || Data[11] != 'E')
	{
		return false;
	}

	bool bFoundFmt = false;
	bool bFoundData = false;
	int32 Offset = 12;
	while (Offset + 8 <= InWavBytes.Num())
	{
		const char ChunkId[5] = { (char)Data[Offset], (char)Data[Offset + 1], (char)Data[Offset + 2], (char)Data[Offset + 3], 0 };
		const uint32 ChunkSize = ReadUint32(Data + Offset + 4);
		const int32 ChunkDataOffset = Offset + 8;

		if (FCStringAnsi::Strcmp(ChunkId, "fmt ") == 0 && ChunkDataOffset + 16 <= InWavBytes.Num())
		{
			OutNumChannels = ReadUint16(Data + ChunkDataOffset + 2);
			OutSampleRate = (int32)ReadUint32(Data + ChunkDataOffset + 4);
			OutBitsPerSample = ReadUint16(Data + ChunkDataOffset + 14);
			bFoundFmt = true;
		}
		else if (FCStringAnsi::Strcmp(ChunkId, "data") == 0)
		{
			const int32 SafeSize = FMath::Min((int32)ChunkSize, InWavBytes.Num() - ChunkDataOffset);
			if (SafeSize > 0)
			{
				OutPCMData.SetNumUninitialized(SafeSize);
				FMemory::Memcpy(OutPCMData.GetData(), Data + ChunkDataOffset, SafeSize);
				bFoundData = true;
			}
		}

		// Chunks are word-aligned.
		Offset = ChunkDataOffset + ChunkSize + (ChunkSize % 2);
	}

	return bFoundFmt && bFoundData;
}
