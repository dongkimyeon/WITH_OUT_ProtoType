#pragma once

#include "CoreMinimal.h"
#include "HAL/Runnable.h"
#include "Containers/Queue.h"

class FSocket;

// Background thread that blocks on FSocket::Recv() and turns the raw byte
// stream into complete, verified Protocol packets (4-byte size prefix +
// FlatBuffers Packet buffer, "PTPK"), matching the server's framing.
//
// Takes its target queues directly (not a UProtoNetClientSubsystem* owner)
// so the SAME class works for either of that subsystem's two independent
// connections (Login and Game -- see 매칭 서버 설계, step 5): each gets its
// own FProtoNetReceiveWorker instance pointed at its own pair of queues,
// with no cross-talk between them.
class FProtoNetReceiveWorker : public FRunnable
{
public:
	FProtoNetReceiveWorker(FSocket* InSocket,
		TQueue<TArray<uint8>, EQueueMode::Mpsc>* InReceivedPackets,
		TQueue<FString, EQueueMode::Mpsc>* InDisconnectReasons);

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
	TQueue<TArray<uint8>, EQueueMode::Mpsc>* ReceivedPackets;
	TQueue<FString, EQueueMode::Mpsc>* DisconnectReasons;
	FThreadSafeBool bStopRequested{false};

	TArray<uint8> Buffer;
	int32 ReadPos = 0;
	int32 WritePos = 0;
};
