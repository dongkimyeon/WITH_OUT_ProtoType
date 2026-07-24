#include "ProtoNetClientSubsystem.h"
#include "ProtoNetReceiveWorker.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/RunnableThread.h"

#include "packet.h"

DEFINE_LOG_CATEGORY_STATIC(LogProtoNet, Log, All);

UProtoNetClientSubsystem::UProtoNetClientSubsystem() = default;
UProtoNetClientSubsystem::~UProtoNetClientSubsystem() = default;
UProtoNetClientSubsystem::UProtoNetClientSubsystem(FVTableHelper& Helper)
	: Super(Helper)
{
}

void UProtoNetClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Auto-connect on game start so the echo server sees a client without
	// needing any Blueprint to call Connect() manually.
	Connect();
}

bool UProtoNetClientSubsystem::Connect(const FString& ServerIp, int32 ServerPort)
{
	if (Socket != nullptr)
	{
		UE_LOG(LogProtoNet, Warning, TEXT("Connect: already connected to a server"));
		return false;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogProtoNet, Error, TEXT("Connect: no socket subsystem"));
		return false;
	}

	bool bIsValidIp = false;
	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetIp(*ServerIp, bIsValidIp);
	Addr->SetPort(ServerPort);
	if (!bIsValidIp)
	{
		UE_LOG(LogProtoNet, Error, TEXT("Connect: invalid server ip '%s'"), *ServerIp);
		return false;
	}

	FSocket* NewSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("ProtoNetClientSocket"), false);
	if (!NewSocket)
	{
		UE_LOG(LogProtoNet, Error, TEXT("Connect: failed to create socket"));
		return false;
	}

	NewSocket->SetNoDelay(true);

	if (!NewSocket->Connect(*Addr))
	{
		UE_LOG(LogProtoNet, Error, TEXT("Connect: failed to connect to %s:%d"), *ServerIp, ServerPort);
		SocketSubsystem->DestroySocket(NewSocket);
		return false;
	}

	Socket = NewSocket;
	Worker = MakeUnique<FProtoNetReceiveWorker>(Socket, this);
	WorkerThread = FRunnableThread::Create(Worker.Get(), TEXT("ProtoNetReceiveWorker"));

	UE_LOG(LogProtoNet, Log, TEXT("Connect: connected to %s:%d"), *ServerIp, ServerPort);
	OnConnected.Broadcast();
	return true;
}

void UProtoNetClientSubsystem::Disconnect()
{
	if (Socket)
	{
		// Unblocks the worker thread's pending Recv() call.
		Socket->Close();
	}

	if (WorkerThread)
	{
		Worker->Stop();
		WorkerThread->WaitForCompletion();
		delete WorkerThread;
		WorkerThread = nullptr;
		Worker.Reset();
	}

	if (Socket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
		Socket = nullptr;
	}
}

bool UProtoNetClientSubsystem::IsConnected() const
{
	return Socket != nullptr && Socket->GetConnectionState() == SCS_Connected;
}

bool UProtoNetClientSubsystem::SendPacketBytes(const TArray<uint8>& PacketBytes)
{
	if (!Socket || PacketBytes.Num() == 0)
		return false;

	FScopeLock Lock(&SendLock);

	const uint8* Data = PacketBytes.GetData();
	const int32 Len = PacketBytes.Num();
	int32 TotalSent = 0;

	while (TotalSent < Len)
	{
		int32 BytesSent = 0;
		if (!Socket->Send(Data + TotalSent, Len - TotalSent, BytesSent) || BytesSent <= 0)
		{
			UE_LOG(LogProtoNet, Warning, TEXT("SendPacketBytes: send failed"));
			return false;
		}
		TotalSent += BytesSent;
	}

	return true;
}

bool UProtoNetClientSubsystem::SendLoginTest(const FString& AuthToken, const FString& ClientVersion)
{
	flatbuffers::FlatBufferBuilder Fbb;
	auto Token = Fbb.CreateString(TCHAR_TO_UTF8(*AuthToken));
	auto Version = Fbb.CreateString(TCHAR_TO_UTF8(*ClientVersion));

	ProtoType::Net::C2S_LoginBuilder LoginBuilder(Fbb);
	LoginBuilder.add_auth_token(Token);
	LoginBuilder.add_client_version(Version);
	auto Login = LoginBuilder.Finish();

	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_Login, Login.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendAttackFire(FVector Origin, FVector Direction, uint8 WeaponSlot)
{
	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		0);
	const ProtoType::Net::Vec3 OriginVec(Origin.X, Origin.Y, Origin.Z);
	const ProtoType::Net::Vec3 DirectionVec(Direction.X, Direction.Y, Direction.Z);

	auto Req = ProtoType::Net::CreateC2S_AttackRequest(
		Fbb, &Header, WeaponSlot, ProtoType::Net::AttackType::Fire, &OriginVec, &DirectionVec);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_AttackRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendInteractLoot(int32 TargetId)
{
	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		0);

	auto Req = ProtoType::Net::CreateC2S_InteractRequest(
		Fbb, &Header, static_cast<uint32>(TargetId), ProtoType::Net::InteractType::Loot);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_InteractRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

void UProtoNetClientSubsystem::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

void UProtoNetClientSubsystem::Tick(float DeltaTime)
{
	TArray<uint8> Packet;
	while (ReceivedPackets.Dequeue(Packet))
	{
		OnPacketReceived.Broadcast(Packet);
	}

	FString Reason;
	if (DisconnectReasons.Dequeue(Reason))
	{
		UE_LOG(LogProtoNet, Warning, TEXT("Disconnected: %s"), *Reason);
		Disconnect();
		OnDisconnected.Broadcast(Reason);
	}
}

TStatId UProtoNetClientSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UProtoNetClientSubsystem, STATGROUP_Tickables);
}

bool UProtoNetClientSubsystem::IsTickable() const
{
	return true;
}
