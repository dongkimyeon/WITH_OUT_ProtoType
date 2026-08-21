// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// PCM/WAV 변환 헬퍼 (STT 브릿지로 보낼 WAV 인코딩, TTS 서버에서 받은 WAV 디코딩에 사용).
namespace CompanionAudioUtils
{
	// 인터리빙된 float32 PCM 샘플을 16bit PCM WAV(RIFF)로 인코딩한다.
	void BuildWavFromFloatPCM(const TArray<float>& InSamples, int32 SampleRate, int32 NumChannels, TArray<uint8>& OutWavBytes);

	// WAV(RIFF) 바이트에서 "fmt "/"data" 청크를 읽어 PCM 데이터만 추출한다. 실패 시 false.
	bool ParseWavPCM(const TArray<uint8>& InWavBytes, TArray<uint8>& OutPCMData, int32& OutSampleRate, int32& OutNumChannels, int32& OutBitsPerSample);
}
