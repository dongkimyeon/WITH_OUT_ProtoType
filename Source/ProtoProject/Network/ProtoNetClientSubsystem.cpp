#include "ProtoNetClientSubsystem.h"
#include "ProtoNetReceiveWorker.h"
#include "ProtoRemotePlayer.h"
#include "ProtoConnectPrompt.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/RunnableThread.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

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
}

void UProtoNetClientSubsystem::ShowConnectPrompt()
{
	if (ConnectPromptWidget.IsValid() || IsConnected())
		return;

	// Pre-fill from -ServerIP= if given, else 127.0.0.1 (this machine).
	FString DefaultIp = TEXT("127.0.0.1");
	FParse::Value(FCommandLine::Get(), TEXT("ServerIP="), DefaultIp);

	SAssignNew(ConnectPromptWidget, SProtoConnectPrompt)
		.InitialServerIp(DefaultIp)
		.OnConnectRequested(FProtoOnConnectRequested::CreateUObject(this, &UProtoNetClientSubsystem::HandleConnectPromptSubmitted));

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->AddViewportWidgetContent(ConnectPromptWidget.ToSharedRef());
	}

	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetShowMouseCursor(true);
			FInputModeUIOnly InputMode;
			InputMode.SetWidgetToFocus(ConnectPromptWidget);
			PC->SetInputMode(InputMode);
		}
	}
}

void UProtoNetClientSubsystem::HandleConnectPromptSubmitted(const FString& ServerIp)
{
	const FString Trimmed = ServerIp.TrimStartAndEnd();

	// Empty or "0" means "skip connecting" - just play offline/local.
	if (Trimmed.IsEmpty() || Trimmed == TEXT("0"))
	{
		HideConnectPrompt();
		return;
	}

	if (ConnectPromptWidget.IsValid())
		ConnectPromptWidget->SetStatusText(FText::FromString(TEXT("접속 중...")));

	// FSocket::Connect() is blocking, so an unreachable IP will briefly
	// freeze the game here rather than failing instantly.
	if (Connect(Trimmed))
	{
		SendLoginTest(TEXT("guest"), TEXT("1.0"));
		HideConnectPrompt();
	}
	else if (ConnectPromptWidget.IsValid())
	{
		ConnectPromptWidget->SetStatusText(FText::FromString(TEXT("접속 실패. IP를 확인하고 다시 시도하세요.")));
	}
}

void UProtoNetClientSubsystem::HideConnectPrompt()
{
	if (ConnectPromptWidget.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
			GEngine->GameViewport->RemoveViewportWidgetContent(ConnectPromptWidget.ToSharedRef());
		ConnectPromptWidget.Reset();
	}

	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetShowMouseCursor(false);
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
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
		LocalPlayerId);
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
		LocalPlayerId);

	auto Req = ProtoType::Net::CreateC2S_InteractRequest(
		Fbb, &Header, static_cast<uint32>(TargetId), ProtoType::Net::InteractType::Loot);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_InteractRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendMoveInput(FVector Position, FRotator Look, int32 Flags)
{
	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		LocalPlayerId);
	const ProtoType::Net::Vec2 MoveInputVec(0.0f, 0.0f);
	const ProtoType::Net::Rotator LookVec(Look.Pitch, Look.Yaw, Look.Roll);
	const ProtoType::Net::Vec3 PositionVec(Position.X, Position.Y, Position.Z);

	auto Req = ProtoType::Net::CreateC2S_MoveInput(
		Fbb, &Header, &MoveInputVec, &LookVec,
		static_cast<ProtoType::Net::MoveFlags>(Flags), &PositionVec);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_MoveInput, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

void UProtoNetClientSubsystem::HandleIncomingPacket(const TArray<uint8>& PacketBytes)
{
	flatbuffers::Verifier Verifier(PacketBytes.GetData(), PacketBytes.Num());
	if (!ProtoType::Net::VerifySizePrefixedPacketBuffer(Verifier))
		return;

	const auto* Packet = ProtoType::Net::GetSizePrefixedPacket(PacketBytes.GetData());

	switch (Packet->payload_type())
	{
		case ProtoType::Net::Payload::S2C_LoginSuccess:
			if (const auto* Success = Packet->payload_as_S2C_LoginSuccess())
			{
				LocalPlayerId = Success->player_id();
				UE_LOG(LogProtoNet, Log, TEXT("Logged in as player %u"), LocalPlayerId);
			}
			break;

		case ProtoType::Net::Payload::S2C_SendPlayerInfo:
			if (const auto* Info = Packet->payload_as_S2C_SendPlayerInfo())
			{
				if (Info->player_id() != LocalPlayerId)
				{
					const auto* Pos = Info->position();
					const auto* Look = Info->look();
					UpdateRemotePlayer(
						Info->player_id(),
						Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector,
						Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator);
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_AttackBroadcast:
			if (const auto* Atk = Packet->payload_as_S2C_AttackBroadcast())
			{
				if (Atk->attacker_id() != LocalPlayerId)
				{
					const auto* Origin = Atk->origin();
					const auto* Direction = Atk->direction();
					if (Origin && Direction)
					{
						const FVector Start(Origin->x(), Origin->y(), Origin->z());
						const FVector Dir(Direction->x(), Direction->y(), Direction->z());
						const FVector End = Start + Dir.GetSafeNormal() * 10000.0f;
						if (UWorld* World = GetWorld())
						{
							// Simple placeholder tracer; swap for a real muzzle
							// flash/impact FX once remote players have a real mesh.
							DrawDebugLine(World, Start, End, FColor::Yellow, false, 1.0f, 0, 1.5f);
						}
					}
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_MoveState:
			if (const auto* State = Packet->payload_as_S2C_MoveState())
			{
				if (State->player_id() != LocalPlayerId)
				{
					const auto* Pos = State->position();
					const auto* Look = State->look();
					UpdateRemotePlayer(
						State->player_id(),
						Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector,
						Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator);
				}
			}
			break;

		default:
			break;
	}
}

void UProtoNetClientSubsystem::UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation)
{
	const int32 Key = static_cast<int32>(PlayerId);

	if (AProtoRemotePlayer** Existing = RemotePlayers.Find(Key))
	{
		if (IsValid(*Existing))
		{
			(*Existing)->SetActorLocationAndRotation(Location, Rotation);
			return;
		}
		RemotePlayers.Remove(Key);
	}

	UWorld* World = GetWorld();
	if (!World)
		return;

	AProtoRemotePlayer* NewRemote = World->SpawnActor<AProtoRemotePlayer>(Location, Rotation);
	if (NewRemote)
	{
		NewRemote->PlayerId = PlayerId;
		RemotePlayers.Add(Key, NewRemote);
		UE_LOG(LogProtoNet, Log, TEXT("Spawned remote player %u"), PlayerId);
	}
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
		HandleIncomingPacket(Packet);
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
