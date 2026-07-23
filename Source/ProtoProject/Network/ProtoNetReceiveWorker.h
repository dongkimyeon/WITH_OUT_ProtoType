#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class FSocket;
class UProtoNetClientSubsystem;

// Background thread that blocks on FSocket::Recv() and turns the raw byte
// stream into complete, verified Protocol packets, matching the framing used
// by the WOP_SERVER RIO echo server: a 4-byte little-endian size prefix
// followed by a size-prefixed FlatBuffers Packet buffer ("PTPK").
class FProtoNetReceiveWorker : public FRunnable
{
public:
	FProtoNetReceiveWorker(FSocket* InSocket, UProtoNetClientSubsystem* InOwner);

	//~ FRunnable
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	//~ End FRunnable

private:
	// Returns false on a protocol violation (caller should disconnect).
	bool ProcessBuffer();

	static constexpr int32 BufferCapacity = 64 * 1024;

	FSocket* Socket;
	UProtoNetClientSubsystem* Owner;
	FThreadSafeBool bStopRequested{false};

	TArray<uint8> Buffer;
	int32 ReadPos = 0;
	int32 WritePos = 0;
};
