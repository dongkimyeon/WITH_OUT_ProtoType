#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"

class FSocket;
class UProtoNetClientSubsystem;

// Background thread that blocks on FSocket::Recv() and turns the raw byte
// stream into complete, verified Protocol packets (4-byte size prefix +
// FlatBuffers Packet buffer, "PTPK"), matching the server's framing.
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
