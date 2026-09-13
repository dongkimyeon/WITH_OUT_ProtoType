#include "ProtoNetReceiveWorker.h"

#include "Sockets.h"

#include "packet.h"

FProtoNetReceiveWorker::FProtoNetReceiveWorker(FSocket* InSocket,
	TQueue<TArray<uint8>, EQueueMode::Mpsc>* InReceivedPackets,
	TQueue<FString, EQueueMode::Mpsc>* InDisconnectReasons)
	: Socket(InSocket)
	, ReceivedPackets(InReceivedPackets)
	, DisconnectReasons(InDisconnectReasons)
{
	Buffer.SetNumUninitialized(BufferCapacity);
}

/*-------------------
 FRunnable 생명주기
-------------------*/
bool FProtoNetReceiveWorker::Init()
{
	return true;
}

void FProtoNetReceiveWorker::Stop()
{
	bStopRequested = true;
}

uint32 FProtoNetReceiveWorker::Run()
{
	for (;;)
	{
		if (WritePos >= BufferCapacity)
		{
			DisconnectReasons->Enqueue(TEXT("receive buffer overflow"));
			return 0;
		}

		int32 BytesRead = 0;
		const bool bOk = Socket->Recv(Buffer.GetData() + WritePos, BufferCapacity - WritePos, BytesRead, ESocketReceiveFlags::None);

		if (bStopRequested)
			return 0;

		if (!bOk || BytesRead <= 0)
		{
			DisconnectReasons->Enqueue(TEXT("connection closed"));
			return 0;
		}

		WritePos += BytesRead;

		if (!ProcessBuffer())
		{
			DisconnectReasons->Enqueue(TEXT("invalid packet framing"));
			return 0;
		}
	}
}

/*-------------------
 수신 버퍼 프레이밍
-------------------*/
bool FProtoNetReceiveWorker::ProcessBuffer()
{
	for (;;)
	{
		const int32 Available = WritePos - ReadPos;
		if (Available < static_cast<int32>(sizeof(uint32)))
			break;

		uint32 BodySize = 0;
		FMemory::Memcpy(&BodySize, Buffer.GetData() + ReadPos, sizeof(BodySize));

		const int64 Total = static_cast<int64>(sizeof(BodySize)) + BodySize;
		if (Total > BufferCapacity)
			return false; // packet larger than the receive buffer

		if (Available < Total)
			break; // wait for the rest of the packet

		flatbuffers::Verifier Verifier(
			reinterpret_cast<const uint8*>(Buffer.GetData() + ReadPos),
			static_cast<size_t>(Total));

		if (!ProtoType::Net::VerifySizePrefixedPacketBuffer(Verifier))
			return false;

		TArray<uint8> PacketCopy;
		PacketCopy.Append(Buffer.GetData() + ReadPos, static_cast<int32>(Total));
		ReceivedPackets->Enqueue(MoveTemp(PacketCopy));

		ReadPos += static_cast<int32>(Total);
	}

	// Compact: slide the unread tail back to the front so the next Recv()
	// always has somewhere to write.
	if (ReadPos > 0)
	{
		const int32 Remaining = WritePos - ReadPos;
		if (Remaining > 0)
			FMemory::Memmove(Buffer.GetData(), Buffer.GetData() + ReadPos, Remaining);
		ReadPos = 0;
		WritePos = Remaining;
	}

	return true;
}
