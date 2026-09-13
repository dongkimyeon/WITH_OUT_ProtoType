#include "ProtoNetClientSubsystem.h"
#include "ProtoNetReceiveWorker.h"
#include "ProtoRemotePlayer.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "../Companion/CompanionNPC.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/RunnableThread.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

#include "packet.h"

DEFINE_LOG_CATEGORY_STATIC(LogProtoNet, Log, All);

/*-------------------
 생성/소멸
-------------------*/
UProtoNetClientSubsystem::UProtoNetClientSubsystem()
{
	// Same Blueprint the local player is spawned as, so remote players look
	// like real characters (mesh + animations) instead of a placeholder.
	static ConstructorHelpers::FClassFinder<AProtoCharacter> CharacterClassFinder(TEXT("/Game/Blueprint/BP_ProtoCharacter"));
	if (CharacterClassFinder.Succeeded())
	{
		RemoteCharacterClass = CharacterClassFinder.Class;
	}
}
UProtoNetClientSubsystem::~UProtoNetClientSubsystem() = default;
UProtoNetClientSubsystem::UProtoNetClientSubsystem(FVTableHelper& Helper)
	: Super(Helper)
{
}

/*-------------------
 UGameInstanceSubsystem 오버라이드
-------------------*/
void UProtoNetClientSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

/*-------------------
 접속 관리
-------------------*/
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
	Worker = MakeUnique<FProtoNetReceiveWorker>(Socket, &ReceivedPackets, &DisconnectReasons);
	WorkerThread = FRunnableThread::Create(Worker.Get(), TEXT("ProtoNetReceiveWorker"));

	UE_LOG(LogProtoNet, Log, TEXT("Connect: connected to %s:%d"), *ServerIp, ServerPort);
	LastServerIp = ServerIp;
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

	// A full Disconnect() means leaving the whole session, not just the
	// Login connection -- take the Game connection (if the ticket flow was
	// ever used) down with it. DisconnectFromGameServer() already handles
	// "was never connected" as a no-op.
	DisconnectFromGameServer();

	// Nothing will tell us about these players again until we reconnect and
	// get a fresh roster -- despawn them now instead of leaving frozen ghosts.
	RemoveAllRemotePlayers();
	LocalPlayerId = 0;

	// A failed/aborted connection shouldn't leak this login's restore data
	// into whatever the next successful login turns out to be.
	bHasPendingProgressRestore = false;
	bPendingProgressApplyTransform = false;
	bHasPendingInventoryRestore = false;
	PendingRestoreInventory.Empty();
	PendingRestoreEquipment.Empty();
	PendingRestoreQuickSlots.Empty();
}

bool UProtoNetClientSubsystem::IsConnected() const
{
	return Socket != nullptr && Socket->GetConnectionState() == SCS_Connected;
}

bool UProtoNetClientSubsystem::RequestMatch()
{
	if (!IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_RequestMatch(Fbb);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_RequestMatch, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	// Always the Login connection, deliberately not SendGameplayPacketBytes:
	// this is a request about the account (which is authenticated on
	// Login), not something a Game connection has any part in yet.
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::ConnectToGameServerAndJoin(const FString& Ticket, const FString& HostOverride, int32 PortOverride)
{
	if (GameSocket != nullptr)
	{
		UE_LOG(LogProtoNet, Warning, TEXT("ConnectToGameServerAndJoin: already connected to a Game server"));
		return false;
	}

	// Empty HostOverride means "same host the Login connection is already
	// on" -- see S2C_MatchTicket's schema comment (today's server always
	// reports an empty game_server_host for the same reason: Login and
	// Game are still the same process/machine). 0 means "the Game server's
	// documented default port" (see WOP_GameServer's main_game.cpp).
	const FString Host = HostOverride.IsEmpty() ? LastServerIp : HostOverride;
	const int32 Port = PortOverride != 0 ? PortOverride : 7778;

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogProtoNet, Error, TEXT("ConnectToGameServerAndJoin: no socket subsystem"));
		return false;
	}

	bool bIsValidIp = false;
	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetIp(*Host, bIsValidIp);
	Addr->SetPort(Port);
	if (!bIsValidIp)
	{
		UE_LOG(LogProtoNet, Error, TEXT("ConnectToGameServerAndJoin: invalid game server ip '%s'"), *Host);
		return false;
	}

	FSocket* NewSocket = SocketSubsystem->CreateSocket(NAME_Stream, TEXT("ProtoNetGameSocket"), false);
	if (!NewSocket)
	{
		UE_LOG(LogProtoNet, Error, TEXT("ConnectToGameServerAndJoin: failed to create socket"));
		return false;
	}

	NewSocket->SetNoDelay(true);

	if (!NewSocket->Connect(*Addr))
	{
		UE_LOG(LogProtoNet, Error, TEXT("ConnectToGameServerAndJoin: failed to connect to %s:%d"), *Host, Port);
		SocketSubsystem->DestroySocket(NewSocket);
		return false;
	}

	GameSocket = NewSocket;
	GameWorker = MakeUnique<FProtoNetReceiveWorker>(GameSocket, &GameReceivedPackets, &GameDisconnectReasons);
	GameWorkerThread = FRunnableThread::Create(GameWorker.Get(), TEXT("ProtoNetGameReceiveWorker"));

	UE_LOG(LogProtoNet, Log, TEXT("ConnectToGameServerAndJoin: connected to %s:%d, redeeming ticket"), *Host, Port);

	flatbuffers::FlatBufferBuilder Fbb;
	auto TicketOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Ticket));
	auto Req = ProtoType::Net::CreateC2S_JoinMatch(Fbb, TicketOffset);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_JoinMatch, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytesOnSocket(GameSocket, GameSendLock, Bytes);
}

void UProtoNetClientSubsystem::DisconnectFromGameServer()
{
	if (GameSocket)
	{
		// Unblocks GameWorker's pending Recv() call.
		GameSocket->Close();
	}

	if (GameWorkerThread)
	{
		GameWorker->Stop();
		GameWorkerThread->WaitForCompletion();
		delete GameWorkerThread;
		GameWorkerThread = nullptr;
		GameWorker.Reset();
	}

	if (GameSocket)
	{
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(GameSocket);
		GameSocket = nullptr;
	}

	// GameSocket->Close() above unblocks GameWorker's pending Recv() with a
	// failure, so by the time WaitForCompletion() returned, Run() almost
	// certainly already pushed its own "connection closed" into
	// GameDisconnectReasons on its way out -- an artifact of THIS
	// intentional teardown, not a genuine surprise drop. Left alone, Tick()
	// would consume it next frame, call DisconnectFromGameServer() a second
	// time (harmless no-op by then) and broadcast OnDisconnectedFromGameServer
	// with a misleading reason on top of whatever this call's actual caller
	// already reported (S2C_JoinMatchFail/HandleMatchTimeout's own
	// OnJoinMatchFailed, or simply "left the raid on purpose" for
	// ExitPoint/RaidManager/Disconnect()) -- drain it here instead.
	FString DiscardedReason;
	while (GameDisconnectReasons.Dequeue(DiscardedReason))
	{
	}

	// Leaving the Game connection means leaving whatever shared raid world
	// it was showing -- same cleanup SetMultiplayerVisualsEnabled(false)
	// already does for the single-connection case.
	RemoveAllRemotePlayers();
}

bool UProtoNetClientSubsystem::IsConnectedToGameServer() const
{
	return GameSocket != nullptr && GameSocket->GetConnectionState() == SCS_Connected;
}

bool UProtoNetClientSubsystem::ConsumePendingProgressRestore(FVector& OutPosition, FRotator& OutLook, uint8& OutWeaponType, bool& bOutApplyTransform)
{
	if (!bHasPendingProgressRestore)
		return false;

	OutPosition = PendingRestorePosition;
	OutLook = PendingRestoreLook;
	OutWeaponType = PendingRestoreWeaponType;
	// false면 위치/시선은 무시하고 무기 타입만 적용한다(레벨 이동 이월).
	bOutApplyTransform = bPendingProgressApplyTransform;
	bHasPendingProgressRestore = false;
	return true;
}

bool UProtoNetClientSubsystem::ConsumePendingInventoryRestore(TArray<FProtoInventoryItemEntry>& OutItems,
	TArray<FProtoEquipmentEntry>& OutEquipment, TArray<FProtoQuickSlotEntry>& OutQuickSlots)
{
	if (!bHasPendingInventoryRestore)
		return false;

	OutItems = PendingRestoreInventory;
	OutEquipment = PendingRestoreEquipment;
	OutQuickSlots = PendingRestoreQuickSlots;
	bHasPendingInventoryRestore = false;
	PendingRestoreInventory.Empty();
	PendingRestoreEquipment.Empty();
	PendingRestoreQuickSlots.Empty();
	return true;
}

void UProtoNetClientSubsystem::SetMultiplayerVisualsEnabled(bool bEnabled)
{
	if (bMultiplayerVisualsEnabled == bEnabled)
		return;

	bMultiplayerVisualsEnabled = bEnabled;

	if (!bMultiplayerVisualsEnabled)
	{
		// Solo map: nothing should still be visible from a stale roster, and
		// we're about to stop processing the packets that would update it.
		RemoveAllRemotePlayers();
	}
	// Turning it back on doesn't need to do anything eagerly -- the next
	// S2C_SendPlayerInfo/S2C_MoveState for each other connected player will
	// (re-)spawn them, same as a fresh login into a multi map.

	// Tell the server too -- we're still connected either way (that's the
	// whole point, so login/progress-save keeps working), but other players
	// need to know we've effectively left/returned, or they'd see our last
	// known position freeze into a ghost instead of despawning it (see
	// C2S_SetVisible's schema comment).
	SendSetVisible(bMultiplayerVisualsEnabled);
}

void UProtoNetClientSubsystem::RemoveAllRemotePlayers()
{
	for (const auto& Pair : RemotePlayers)
	{
		if (IsValid(Pair.Value))
		{
			Pair.Value->Destroy();
		}
	}
	RemotePlayers.Empty();
	RemoteTargetLocation.Empty();
	RemoteTargetRotation.Empty();
}

/*-------------------
 패킷 송신 헬퍼
-------------------*/
bool UProtoNetClientSubsystem::SendPacketBytesOnSocket(FSocket* TargetSocket, FCriticalSection& Lock, const TArray<uint8>& PacketBytes)
{
	if (!TargetSocket || PacketBytes.Num() == 0)
		return false;

	FScopeLock ScopeLock(&Lock);

	const uint8* Data = PacketBytes.GetData();
	const int32 Len = PacketBytes.Num();
	int32 TotalSent = 0;

	while (TotalSent < Len)
	{
		int32 BytesSent = 0;
		if (!TargetSocket->Send(Data + TotalSent, Len - TotalSent, BytesSent) || BytesSent <= 0)
		{
			UE_LOG(LogProtoNet, Warning, TEXT("SendPacketBytesOnSocket: send failed"));
			return false;
		}
		TotalSent += BytesSent;
	}

	return true;
}

bool UProtoNetClientSubsystem::SendPacketBytes(const TArray<uint8>& PacketBytes)
{
	// Always the Login connection -- see this function's header comment
	// and SendGameplayPacketBytes below for the Game-preferring one every
	// gameplay Send*() helper actually uses.
	return SendPacketBytesOnSocket(Socket, SendLock, PacketBytes);
}

bool UProtoNetClientSubsystem::SendGameplayPacketBytes(const TArray<uint8>& PacketBytes)
{
	if (GameSocket)
		return SendPacketBytesOnSocket(GameSocket, GameSendLock, PacketBytes);

	// No Game connection ever established (ConnectToGameServerAndJoin was
	// never called, or it was and then DisconnectFromGameServer tore it
	// back down) -- fall back to the Login connection, reproducing
	// today's single-connection behavior exactly. See this function's
	// header comment.
	return SendPacketBytesOnSocket(Socket, SendLock, PacketBytes);
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

bool UProtoNetClientSubsystem::SendAccountLogin(const FString& Username, const FString& Password, bool bIsRegister)
{
	flatbuffers::FlatBufferBuilder Fbb;
	auto Token = Fbb.CreateString("");
	auto Version = Fbb.CreateString("1.0");
	auto UsernameOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Username));
	auto PasswordOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Password));

	auto Login = ProtoType::Net::CreateC2S_Login(Fbb, Token, Version, UsernameOffset, PasswordOffset, bIsRegister);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_Login, Login.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::ConnectAndLogin(const FString& ServerIp, const FString& Username, const FString& Password)
{
	if (!IsConnected() && !Connect(ServerIp))
		return false;

	LastServerIp = ServerIp;
	LastUsername = Username;
	return SendAccountLogin(Username, Password, /*bIsRegister=*/false);
}

bool UProtoNetClientSubsystem::ConnectAndRegister(const FString& ServerIp, const FString& Username, const FString& Password)
{
	if (!IsConnected() && !Connect(ServerIp))
		return false;

	LastServerIp = ServerIp;
	LastUsername = Username;
	return SendAccountLogin(Username, Password, /*bIsRegister=*/true);
}

bool UProtoNetClientSubsystem::SendAttackFire(FVector Origin, FVector Direction, uint8 WeaponSlot)
{
	if (!bMultiplayerVisualsEnabled)
		return false;

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
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendInteractLoot(int32 TargetId)
{
	if (!bMultiplayerVisualsEnabled)
		return false;

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
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendDoorInteract(int32 DoorId, bool bOpen)
{
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		LocalPlayerId);

	const auto InteractType = bOpen ? ProtoType::Net::InteractType::DoorOpen : ProtoType::Net::InteractType::DoorClose;
	auto Req = ProtoType::Net::CreateC2S_InteractRequest(Fbb, &Header, static_cast<uint32_t>(DoorId), InteractType);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_InteractRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::TryGetCachedDoorState(int32 DoorId, bool& OutIsOpen) const
{
	if (const bool* Found = CachedDoorStates.Find(DoorId))
	{
		OutIsOpen = *Found;
		return true;
	}
	return false;
}

bool UProtoNetClientSubsystem::SendMoveInput(FVector Position, FRotator Look, int32 Flags)
{
	if (!bMultiplayerVisualsEnabled)
		return false;

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
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendWeaponReload(uint8 WeaponType)
{
	if (!bMultiplayerVisualsEnabled)
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		LocalPlayerId);

	auto Req = ProtoType::Net::CreateC2S_ItemUseRequest(
		Fbb, &Header, 0, WeaponType, ProtoType::Net::ItemUseType::Reload);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_ItemUseRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendWeaponEquip(uint8 WeaponType)
{
	if (!bMultiplayerVisualsEnabled)
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Header Header(
		NextSeq++,
		static_cast<uint32>(FDateTime::Now().GetTicks() / ETimespan::TicksPerMillisecond),
		LocalPlayerId);

	auto Req = ProtoType::Net::CreateC2S_ItemUseRequest(
		Fbb, &Header, 0, WeaponType, ProtoType::Net::ItemUseType::Equip);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_ItemUseRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendSaveInventory(const TArray<FProtoInventoryItemEntry>& Items)
{
	// Not gated by bMultiplayerVisualsEnabled -- see the header comment.
	// Still requires a real login (LocalPlayerId==0 before that), same as
	// every other Send* here would be meaningless without one.
	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::InventoryItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoInventoryItemEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		ItemOffsets.Add(ProtoType::Net::CreateInventoryItemEntry(
			Fbb, ItemIdOffset,
			static_cast<int16_t>(Item.GridX), static_cast<int16_t>(Item.GridY),
			Item.bRotated, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_SaveInventory(Fbb, ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_SaveInventory, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendSaveEquipment(const TArray<FProtoEquipmentEntry>& Items)
{
	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::EquipmentItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoEquipmentEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		ItemOffsets.Add(ProtoType::Net::CreateEquipmentItemEntry(Fbb, static_cast<uint8_t>(Item.Slot), ItemIdOffset));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_SaveEquipment(Fbb, ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_SaveEquipment, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendSaveQuickSlots(const TArray<FProtoQuickSlotEntry>& Items)
{
	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::QuickSlotItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoQuickSlotEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		ItemOffsets.Add(ProtoType::Net::CreateQuickSlotItemEntry(
			Fbb, static_cast<uint8_t>(Item.SlotIndex), ItemIdOffset, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_SaveQuickSlots(Fbb, ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_SaveQuickSlots, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendRequestStash(int32 StashIndex)
{
	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_RequestStash(Fbb, static_cast<uint8_t>(StashIndex));
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_RequestStash, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendSaveStash(int32 StashIndex, const TArray<FProtoInventoryItemEntry>& Items)
{
	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::InventoryItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoInventoryItemEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		ItemOffsets.Add(ProtoType::Net::CreateInventoryItemEntry(
			Fbb, ItemIdOffset,
			static_cast<int16_t>(Item.GridX), static_cast<int16_t>(Item.GridY),
			Item.bRotated, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_SaveStash(Fbb, static_cast<uint8_t>(StashIndex), ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_SaveStash, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendDropItem(FName ItemId, FVector Position, int32 StackCount, int32& OutNetSlotId)
{
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	const uint32 Sequence = NextDropSequence++;
	OutNetSlotId = ComputeItemDropNetSlotId(LocalPlayerId, Sequence);

	flatbuffers::FlatBufferBuilder Fbb;
	auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*ItemId.ToString()));
	const ProtoType::Net::Vec3 PositionVec(Position.X, Position.Y, Position.Z);
	auto ItemEntry = ProtoType::Net::CreateWorldSpawnedItemEntry(Fbb, ItemIdOffset, &PositionVec, static_cast<int16_t>(StackCount));

	auto Req = ProtoType::Net::CreateC2S_DropItem(Fbb, Sequence, ItemEntry);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_DropItem, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendSetVisible(bool bVisible)
{
	// Not gated by bMultiplayerVisualsEnabled -- this call is what changes
	// that flag in the first place (see SetMultiplayerVisualsEnabled).
	if (!IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_SetVisible(Fbb, bVisible);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_SetVisible, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendPlayerDied(const TArray<FProtoWorldItemEntry>& Items)
{
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::WorldSpawnedItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoWorldItemEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		const ProtoType::Net::Vec3 PositionVec(Item.Position.X, Item.Position.Y, Item.Position.Z);
		ItemOffsets.Add(ProtoType::Net::CreateWorldSpawnedItemEntry(
			Fbb, ItemIdOffset, &PositionVec, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_PlayerDied(Fbb, ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_PlayerDied, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendContainerLootRoll(int32 ContainerId, const TArray<FProtoInventoryItemEntry>& Items)
{
	// Gated by bMultiplayerVisualsEnabled (see the header comment): a
	// Single map's containerId lives in the SAME server-wide, process-
	// lifetime containerLoot_ map a Multi map's does (keyed only by the
	// placed actor's name hash, with no per-session/per-map partitioning),
	// so without this, two clients each thinking they're in their own
	// private Single-map run would actually be racing each other's rolls.
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::InventoryItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoInventoryItemEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		ItemOffsets.Add(ProtoType::Net::CreateInventoryItemEntry(
			Fbb, ItemIdOffset,
			static_cast<int16_t>(Item.GridX), static_cast<int16_t>(Item.GridY),
			Item.bRotated, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_ContainerLootRoll(Fbb, static_cast<uint32_t>(ContainerId), ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_ContainerLootRoll, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendItemSpawnRoll(int32 SpawnPointId, const TArray<FProtoWorldItemEntry>& Items)
{
	// Same reasoning as SendContainerLootRoll's gate -- itemSpawnRolls_ is
	// the same kind of server-wide, unpartitioned map.
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;

	TArray<flatbuffers::Offset<ProtoType::Net::WorldSpawnedItemEntry>> ItemOffsets;
	ItemOffsets.Reserve(Items.Num());
	for (const FProtoWorldItemEntry& Item : Items)
	{
		auto ItemIdOffset = Fbb.CreateString(TCHAR_TO_UTF8(*Item.ItemId.ToString()));
		const ProtoType::Net::Vec3 PositionVec(Item.Position.X, Item.Position.Y, Item.Position.Z);
		ItemOffsets.Add(ProtoType::Net::CreateWorldSpawnedItemEntry(
			Fbb, ItemIdOffset, &PositionVec, static_cast<int16_t>(Item.StackCount)));
	}
	auto ItemsVector = Fbb.CreateVector(ItemOffsets.GetData(), ItemOffsets.Num());

	auto Req = ProtoType::Net::CreateC2S_ItemSpawnRoll(Fbb, static_cast<uint32_t>(SpawnPointId), ItemsVector);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_ItemSpawnRoll, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendCompanionMoveInput(FVector Position, FRotator Look, float Health, bool bIsDead,
	uint8 WeaponType, bool bIsAiming, float AimPitch)
{
	// Gated by bMultiplayerVisualsEnabled after all -- see this function's
	// header comment for why this used to be the one exception, and
	// UProtoNetClientSubsystem::SetMultiplayerVisualsEnabled for why that was
	// wrong: leaving Multi (SafePlace/ExitPoint/RaidManager) properly tells
	// the server this session is invisible and everyone else despawns our
	// remote player + companion via S2C_PlayerLeft, but this companion actor
	// itself keeps existing and ticking regardless (it isn't destroyed on
	// leaving). Without this gate it kept broadcasting C2S_CompanionMoveInput
	// from wherever it physically is now (SafePlace, a different Single map,
	// etc.), and the server's relay (server_.Broadcast, no visibility filter
	// -- see EchoServer::SnapshotOtherSessions) happily forwarded that to
	// every still-visible Multi-map client, which promptly re-spawned the
	// companion puppet S2C_PlayerLeft had just despawned -- a companion
	// "ghost" that kept reappearing at the wrong location every
	// NetSyncInterval, and (since AEnemyBase::UpdateTarget() scans for every
	// ACompanionNPC in the world) could pull a zombie's attention toward that
	// stale, nobody's-really-there position too. Same gate as every other
	// Send*() helper now.
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Vec3 PositionVec(Position.X, Position.Y, Position.Z);
	const ProtoType::Net::Rotator LookRot(Look.Pitch, Look.Yaw, Look.Roll);
	auto Req = ProtoType::Net::CreateC2S_CompanionMoveInput(Fbb, &PositionVec, &LookRot, Health, bIsDead,
		WeaponType, bIsAiming, AimPitch);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_CompanionMoveInput, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendEnemyClaimRequest(int32 EnemyId)
{
	// Gated by bMultiplayerVisualsEnabled: enemyOwners_ is a server-wide,
	// process-lifetime map (enemy_id -> owning session), with no per-
	// session/per-map partitioning -- without this, two clients each in
	// what they think is their own private Single map would compete for
	// the SAME enemy_id's ownership (same placed-actor name hash), and the
	// loser would just mirror the winner's unrelated game instead of
	// running its own local AI. A Single map never actually needed the
	// claim round trip anyway: bIsNetworkOwner defaults to true and simply
	// stays true forever when this never gets sent, which is exactly
	// "run your own local AI, no one else to arbitrate against."
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_EnemyClaimRequest(Fbb, static_cast<uint32_t>(EnemyId));
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_EnemyClaimRequest, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendEnemyState(int32 EnemyId, FVector Position, FRotator Look, float Health, bool bIsDead)
{
	// See SendEnemyClaimRequest's gate: with claiming gated off, a Single
	// map's enemies are always bIsNetworkOwner=true and would otherwise
	// keep calling this every tick regardless -- broadcasting a Single-map
	// zombie's state to every other connected session (including other
	// Single-map players' own, unrelated runs) serves no purpose there.
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Vec3 PositionVec(Position.X, Position.Y, Position.Z);
	const ProtoType::Net::Rotator LookRot(Look.Pitch, Look.Yaw, Look.Roll);
	auto Req = ProtoType::Net::CreateC2S_EnemyState(Fbb, static_cast<uint32_t>(EnemyId), &PositionVec, &LookRot, Health, bIsDead);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_EnemyState, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendEnemyDamage(int32 EnemyId, float Damage)
{
	// See SendEnemyClaimRequest's gate -- this is only ever meaningful
	// against a claimed owner elsewhere, which can't exist once claiming
	// itself is gated off for a Single map.
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_EnemyDamage(Fbb, static_cast<uint32_t>(EnemyId), Damage);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_EnemyDamage, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendEnemyRegister(int32 EnemyId, FVector Position, float Health, float MaxHealth, float MoveSpeed, float AttackRange, float AttackDamage, float AttackCooldown, bool bIsCaller, float CallRadius, float CallCooldown)
{
	if (!IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	const ProtoType::Net::Vec3 PositionVec(Position.X, Position.Y, Position.Z);
	auto Req = ProtoType::Net::CreateC2S_EnemyRegister(Fbb, static_cast<uint32_t>(EnemyId), &PositionVec, Health, MaxHealth, MoveSpeed, AttackRange, AttackDamage, AttackCooldown, bIsCaller, CallRadius, CallCooldown);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_EnemyRegister, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendGameplayPacketBytes(Bytes);
}

void UProtoNetClientSubsystem::CacheStateForLevelTransition(uint8 WeaponType,
	const TArray<FProtoInventoryItemEntry>& InventoryItems, const TArray<FProtoEquipmentEntry>& Equipment,
	const TArray<FProtoQuickSlotEntry>& QuickSlots)
{
	// 레벨 이동(LevelChanger / ExitPoint)은 목적지의 PlayerStart에서 시작해야 한다.
	// 위치/시선은 이월하지 않고(bPendingProgressApplyTransform = false), 장착 무기 타입만
	// 이월해 다음 레벨에서 손에 든 무기 비주얼이 유지되게 한다. 서버 로그인
	// (S2C_LoginSuccess + has_saved_progress) 쪽도 동일하게 항상 false -- 어느 레벨
	// 좌표인지 알 수 없는 저장 위치를 그대로 적용하면 안 된다(강제종료 버그, 그쪽
	// 핸들러의 주석 참고). 결과적으로 위치/시선은 이제 어떤 경로로도 절대 적용되지
	// 않는다 -- 모든 진입은 항상 그 레벨 자신의 PlayerStart에서 시작한다.
	bHasPendingProgressRestore = true;
	PendingRestorePosition = FVector::ZeroVector;
	PendingRestoreLook = FRotator::ZeroRotator;
	PendingRestoreWeaponType = WeaponType;
	bPendingProgressApplyTransform = false;

	bHasPendingInventoryRestore = true;
	PendingRestoreInventory = InventoryItems;
	PendingRestoreEquipment = Equipment;
	PendingRestoreQuickSlots = QuickSlots;

	UE_LOG(LogProtoNet, Log, TEXT("[InvSync] CacheStateForLevelTransition: cached %d item(s), %d equipment, %d quick slot(s)"),
		InventoryItems.Num(), Equipment.Num(), QuickSlots.Num());
}

/*-------------------
 수신 패킷 처리
-------------------*/
void UProtoNetClientSubsystem::HandleIncomingPacket(const TArray<uint8>& PacketBytes)
{
	flatbuffers::Verifier Verifier(PacketBytes.GetData(), PacketBytes.Num());
	if (!ProtoType::Net::VerifySizePrefixedPacketBuffer(Verifier))
		return;

	const auto* Packet = ProtoType::Net::GetSizePrefixedPacket(PacketBytes.GetData());

	// Solo map (see SetMultiplayerVisualsEnabled): ignore every packet type
	// that would spawn/update/remove another player's actor, or draw their
	// attack tracer/hit marker. Login/connection packets fall through to the
	// switch below as normal -- this isn't a "go offline" toggle.
	if (!bMultiplayerVisualsEnabled)
	{
		switch (Packet->payload_type())
		{
			case ProtoType::Net::Payload::S2C_SendPlayerInfo:
			case ProtoType::Net::Payload::S2C_AttackBroadcast:
			case ProtoType::Net::Payload::S2C_MoveState:
			case ProtoType::Net::Payload::S2C_ItemUseBroadcast:
			case ProtoType::Net::Payload::S2C_PlayerLeft:
			case ProtoType::Net::Payload::S2C_AttackResult:
			case ProtoType::Net::Payload::S2C_CompanionMoveState:
			case ProtoType::Net::Payload::S2C_PlayerDied:
			case ProtoType::Net::Payload::S2C_EnemyAttackResult:
				// Defense in depth alongside EchoServer::EnemyAiLoop's own
				// visibility filter (see Session::IsVisible's comment): a
				// stray hit computed the same server tick this client's
				// C2S_SetVisible(false) is still in flight would otherwise
				// land HandleEnemyAttackPlayer damage on a player who isn't
				// even in a zombie-having level anymore.
			case ProtoType::Net::Payload::S2C_ItemDropped:
				// Another player's manual world-drop (see SendDropItem/
				// HandleItemDropped) -- was missing from this list entirely
				// when C2S_DropItem/S2C_ItemDropped was added, so a player who
				// left to a Solo map/SafePlace while still connected would
				// spawn a teammate's just-dropped item into the WRONG level,
				// at that other map's coordinates, exactly the same "leftover
				// state leaks across the visibility boundary" bug as
				// SendCompanionMoveInput's (see that function's comment).
			case ProtoType::Net::Payload::S2C_EnemyState:
			case ProtoType::Net::Payload::S2C_EnemyAttackBroadcast:
				// Same defense-in-depth reasoning as S2C_EnemyAttackResult
				// above -- these only ever affect an AEnemyBase this client
				// has loaded (matched by GetEnemyId()), so this is currently
				// a no-op in practice, but cheap insurance against ever
				// mirroring/animating a zombie belonging to a level this
				// client isn't in anymore.
				return;
			default:
				break;
		}
	}

	switch (Packet->payload_type())
	{
		case ProtoType::Net::Payload::S2C_LoginSuccess:
			if (const auto* Success = Packet->payload_as_S2C_LoginSuccess())
			{
				LocalPlayerId = Success->player_id();
				UE_LOG(LogProtoNet, Log, TEXT("Logged in as player %u"), LocalPlayerId);

				if (Success->has_saved_progress())
				{
					const auto* Pos = Success->position();
					const auto* Look = Success->look();
					const FVector RestoredPosition = Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector;
					const FRotator RestoredLook = Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator;

					// Cached in addition to broadcasting: if this login came
					// from TitleLevel, no AProtoCharacter exists to catch the
					// broadcast yet (it only spawns after the level travel
					// HandleLoginSucceeded triggers) -- see
					// ConsumePendingProgressRestore's header comment.
					bHasPendingProgressRestore = true;
					PendingRestorePosition = RestoredPosition;
					PendingRestoreLook = RestoredLook;
					PendingRestoreWeaponType = Success->weapon_type();
					// 항상 false: 로그인은 무조건 SafePlaceLevel로 이동하는데
					// (TitleLevelWidget::HandleLoginSucceeded), PlayerProgress에는 이 위치가
					// 어느 레벨 좌표인지 정보가 없다 -- 레이드(싱글/멀티맵) 도중 강제종료하면
					// 그 좌표가 그대로 저장되고, 다음 로그인 때 SafePlaceLevel의 전혀 다른
					// 지형에 그대로 적용되어 "강제종료한 플레이어의 위치가 이상한곳으로
					// 고정되는 문제"가 발생했다. 레벨 이동 이월(CacheStateForLevelTransition)과
					// 마찬가지로 항상 PlayerStart에서 시작하고, WeaponType만 이월한다.
					bPendingProgressApplyTransform = false;

					OnProgressRestored.Broadcast(RestoredPosition, RestoredLook, Success->weapon_type());
				}

				// Unlike OnProgressRestored above, this fires on EVERY login,
				// not just has_saved_progress ones -- there's no default
				// starting inventory to protect (see ProtoCharacter.cpp's
				// commented-out TestRifle/TestBandage/TestArmor seeding), and
				// this same running client may have just logged out of a
				// DIFFERENT account that left items sitting in the grid. The
				// listener (HandleInventoryRestored) always clears first, so
				// an empty array here correctly means "this account has
				// nothing saved" instead of "leave whatever's already there".
				{
					TArray<FProtoInventoryItemEntry> InventoryItems;
					if (const auto* Inventory = Success->inventory())
					{
						InventoryItems.Reserve(Inventory->size());
						for (const auto* Entry : *Inventory)
						{
							if (!Entry || !Entry->item_id())
								continue;
							FProtoInventoryItemEntry ItemEntry;
							ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
							ItemEntry.GridX = Entry->grid_x();
							ItemEntry.GridY = Entry->grid_y();
							ItemEntry.bRotated = Entry->rotated();
							ItemEntry.StackCount = Entry->stack_count();
							InventoryItems.Add(ItemEntry);
						}
					}
					TArray<FProtoEquipmentEntry> EquipmentItems;
					if (const auto* Equipment = Success->equipment())
					{
						EquipmentItems.Reserve(Equipment->size());
						for (const auto* Entry : *Equipment)
						{
							if (!Entry || !Entry->item_id())
								continue;
							FProtoEquipmentEntry ItemEntry;
							ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
							ItemEntry.Slot = Entry->slot();
							EquipmentItems.Add(ItemEntry);
						}
					}

					TArray<FProtoQuickSlotEntry> QuickSlotItems;
					if (const auto* QuickSlots = Success->quick_slots())
					{
						QuickSlotItems.Reserve(QuickSlots->size());
						for (const auto* Entry : *QuickSlots)
						{
							if (!Entry || !Entry->item_id())
								continue;
							FProtoQuickSlotEntry ItemEntry;
							ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
							ItemEntry.SlotIndex = Entry->slot_index();
							ItemEntry.StackCount = Entry->stack_count();
							QuickSlotItems.Add(ItemEntry);
						}
					}

					// Cached in addition to broadcasting -- same reason as
					// PendingRestorePosition/Look/WeaponType above. Any stale
					// level-transition cache from before this login is
					// overwritten here (not merely left alone) either way, so
					// it can't leak onto whichever account just logged in.
					bHasPendingInventoryRestore = true;
					PendingRestoreInventory = InventoryItems;
					PendingRestoreEquipment = EquipmentItems;
					PendingRestoreQuickSlots = QuickSlotItems;

					OnInventoryRestored.Broadcast(InventoryItems);
				}

				OnLoginSucceeded.Broadcast(static_cast<int32>(LocalPlayerId), Success->has_saved_progress());
			}
			break;

		case ProtoType::Net::Payload::S2C_LoginFail:
			if (const auto* Fail = Packet->payload_as_S2C_LoginFail())
			{
				const FString Message = Fail->message() ? UTF8_TO_TCHAR(Fail->message()->c_str()) : FString();
				UE_LOG(LogProtoNet, Warning, TEXT("Login failed: %s"), *Message);

				OnLoginFailed.Broadcast(static_cast<EProtoLoginFailReason>(Fail->reason()), Message);
			}
			break;

		case ProtoType::Net::Payload::S2C_MatchTicket:
			if (const auto* TicketMsg = Packet->payload_as_S2C_MatchTicket())
			{
				const FString Host = TicketMsg->game_server_host() ? UTF8_TO_TCHAR(TicketMsg->game_server_host()->c_str()) : FString();
				const FString Ticket = TicketMsg->ticket() ? UTF8_TO_TCHAR(TicketMsg->ticket()->c_str()) : FString();
				UE_LOG(LogProtoNet, Log, TEXT("Received match ticket (game server port %d)"), TicketMsg->game_server_port());

				OnMatchTicketReceived.Broadcast(Host, static_cast<int32>(TicketMsg->game_server_port()), Ticket,
					static_cast<int64>(TicketMsg->expires_at_unix_ms()));
			}
			break;

		case ProtoType::Net::Payload::S2C_JoinMatchFail:
			if (const auto* JoinFail = Packet->payload_as_S2C_JoinMatchFail())
			{
				UE_LOG(LogProtoNet, Warning, TEXT("C2S_JoinMatch failed (reason %d)"), static_cast<int32>(JoinFail->reason()));
				OnJoinMatchFailed.Broadcast(static_cast<EProtoJoinMatchFailReason>(JoinFail->reason()));

				// The server replies with this WITHOUT closing the socket
				// (see its C2S_JoinMatch case's comment -- it leaves room
				// for a retry on the same connection), so GameSocket is
				// still non-null and "connected" at the TCP level here.
				// SendGameplayPacketBytes only checks for a non-null
				// GameSocket, not whether the join actually succeeded --
				// without tearing it down, every gameplay packet from now
				// on would keep silently going to this dead-end session
				// (never authenticated, no Room -- every C2S_* case's own
				// "if (!room) break;" just drops it) instead of falling
				// back to the Login connection, breaking the graceful-
				// degradation this whole ticket flow was designed around.
				DisconnectFromGameServer();
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
					const bool bSprinting = (static_cast<uint16>(State->flags()) & static_cast<uint16>(ProtoType::Net::MoveFlags::Sprint)) != 0;
					const bool bAiming = (static_cast<uint16>(State->flags()) & static_cast<uint16>(ProtoType::Net::MoveFlags::ADS)) != 0;
					UpdateRemotePlayer(
						State->player_id(),
						Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector,
						Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator,
						bSprinting,
						bAiming);
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_ItemUseBroadcast:
			if (const auto* Use = Packet->payload_as_S2C_ItemUseBroadcast())
			{
				if (Use->user_id() != LocalPlayerId)
				{
					if (AActor** Existing = RemotePlayers.Find(static_cast<int32>(Use->user_id())))
					{
						if (AProtoCharacter* RemoteCharacter = Cast<AProtoCharacter>(*Existing))
						{
							if (Use->use_type() == ProtoType::Net::ItemUseType::Reload)
							{
								RemoteCharacter->PlayRemoteReloadMontage(static_cast<EWeaponType>(Use->slot()));
							}
							else if (Use->use_type() == ProtoType::Net::ItemUseType::Equip)
							{
								RemoteCharacter->ApplyRemoteWeaponEquip(static_cast<EWeaponType>(Use->slot()));
							}
						}
					}
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_PlayerLeft:
			if (const auto* Left = Packet->payload_as_S2C_PlayerLeft())
			{
				RemoveRemotePlayer(Left->player_id());
				RemoveRemoteCompanion(Left->player_id());
			}
			break;

		case ProtoType::Net::Payload::S2C_PlayerDied:
			if (const auto* Died = Packet->payload_as_S2C_PlayerDied())
			{
				// Never fires for the local player (HandleDeath() calls
				// SendPlayerDied() itself, the server doesn't echo it
				// back) -- only ever a remote mirror, if one is currently
				// spawned for this player.
				if (AActor** Existing = RemotePlayers.Find(static_cast<int32>(Died->player_id())))
				{
					if (AProtoCharacter* RemoteCharacter = Cast<AProtoCharacter>(*Existing))
					{
						TArray<FProtoWorldItemEntry> Items;
						if (const auto* Entries = Died->items())
						{
							Items.Reserve(Entries->size());
							for (const auto* Entry : *Entries)
							{
								if (!Entry || !Entry->item_id())
									continue;
								FProtoWorldItemEntry ItemEntry;
								ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
								if (const auto* Pos = Entry->position())
								{
									ItemEntry.Position = FVector(Pos->x(), Pos->y(), Pos->z());
								}
								ItemEntry.StackCount = Entry->stack_count();
								Items.Add(ItemEntry);
							}
						}
						RemoteCharacter->HandleRemotePlayerDied(static_cast<int32>(Died->player_id()), Items);
					}
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_AttackResult:
			if (const auto* Result = Packet->payload_as_S2C_AttackResult())
			{
				if (Result->hit())
				{
					if (const auto* HitPos = Result->hit_position())
					{
						if (UWorld* World = GetWorld())
						{
							DrawDebugSphere(World, FVector(HitPos->x(), HitPos->y(), HitPos->z()), 20.0f, 8, FColor::Red, false, 1.0f, 0, 2.0f);
						}
					}
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_StashState:
			if (const auto* State = Packet->payload_as_S2C_StashState())
			{
				// Unicast to this account only -- but every AStorageContainer
				// this client placed shares this one delegate, so StashIndex
				// still needs to be broadcast for each to filter on (see
				// this delegate's comment).
				TArray<FProtoInventoryItemEntry> Items;
				if (const auto* Entries = State->items())
				{
					Items.Reserve(Entries->size());
					for (const auto* Entry : *Entries)
					{
						if (!Entry || !Entry->item_id())
							continue;
						FProtoInventoryItemEntry ItemEntry;
						ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
						ItemEntry.GridX = Entry->grid_x();
						ItemEntry.GridY = Entry->grid_y();
						ItemEntry.bRotated = Entry->rotated();
						ItemEntry.StackCount = Entry->stack_count();
						Items.Add(ItemEntry);
					}
				}
				OnStashState.Broadcast(static_cast<int32>(State->stash_index()), Items);
			}
			break;

		case ProtoType::Net::Payload::S2C_ItemDropped:
			if (const auto* Dropped = Packet->payload_as_S2C_ItemDropped())
			{
				// Server already excludes the dropper from this broadcast
				// (see this message's schema comment) -- the player_id
				// check is defense in depth, same as other broadcasts'.
				if (Dropped->player_id() != LocalPlayerId && Dropped->item() && Dropped->item()->item_id())
				{
					const auto* Item = Dropped->item();
					const int32 NetSlotId = ComputeItemDropNetSlotId(Dropped->player_id(), Dropped->drop_sequence());
					const FName ItemId(UTF8_TO_TCHAR(Item->item_id()->c_str()));
					const FVector Position = Item->position()
						? FVector(Item->position()->x(), Item->position()->y(), Item->position()->z())
						: FVector::ZeroVector;
					OnItemDropped.Broadcast(NetSlotId, ItemId, Position, Item->stack_count());
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_ContainerLootState:
			if (const auto* State = Packet->payload_as_S2C_ContainerLootState())
			{
				TArray<FProtoInventoryItemEntry> Items;
				if (const auto* Entries = State->items())
				{
					Items.Reserve(Entries->size());
					for (const auto* Entry : *Entries)
					{
						if (!Entry || !Entry->item_id())
							continue;
						FProtoInventoryItemEntry ItemEntry;
						ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
						ItemEntry.GridX = Entry->grid_x();
						ItemEntry.GridY = Entry->grid_y();
						ItemEntry.bRotated = Entry->rotated();
						ItemEntry.StackCount = Entry->stack_count();
						Items.Add(ItemEntry);
					}
				}
				OnContainerLootState.Broadcast(static_cast<int32>(State->container_id()), Items);
			}
			break;

		case ProtoType::Net::Payload::S2C_ItemSpawnState:
			if (const auto* State = Packet->payload_as_S2C_ItemSpawnState())
			{
				TArray<FProtoWorldItemEntry> Items;
				if (const auto* Entries = State->items())
				{
					Items.Reserve(Entries->size());
					for (const auto* Entry : *Entries)
					{
						if (!Entry || !Entry->item_id())
							continue;
						FProtoWorldItemEntry ItemEntry;
						ItemEntry.ItemId = FName(UTF8_TO_TCHAR(Entry->item_id()->c_str()));
						if (const auto* Pos = Entry->position())
						{
							ItemEntry.Position = FVector(Pos->x(), Pos->y(), Pos->z());
						}
						ItemEntry.StackCount = Entry->stack_count();
						Items.Add(ItemEntry);
					}
				}
				OnItemSpawnState.Broadcast(static_cast<int32>(State->spawn_point_id()), Items);
			}
			break;

		case ProtoType::Net::Payload::S2C_InteractResult:
			if (const auto* Result = Packet->payload_as_S2C_InteractResult())
			{
				// Door open/close and Loot are the only interact_types with
				// client-side meaning right now -- see SendDoorInteract's/
				// OnItemPickupResult's schema comments. Extract/PlantItem/
				// UseSwitch are silently ignored here, same as the server
				// treats them.
				if (Result->interact_type() == ProtoType::Net::InteractType::DoorOpen
					|| Result->interact_type() == ProtoType::Net::InteractType::DoorClose)
				{
					const int32 DoorId = static_cast<int32>(Result->target_id());
					const bool bOpen = Result->interact_type() == ProtoType::Net::InteractType::DoorOpen;

					// Cache first: this same message shape is also how
					// C2S_Login's roster loop replays already-toggled doors
					// to a newly-joining client, and that replay can arrive
					// before the matching ADoor even exists yet to catch
					// the broadcast below -- see TryGetCachedDoorState.
					CachedDoorStates.Add(DoorId, bOpen);
					OnDoorInteract.Broadcast(DoorId, bOpen);
				}
				else if (Result->interact_type() == ProtoType::Net::InteractType::Loot)
				{
					// Both Ok and Denied are delegated now -- see
					// OnItemPickupResult's schema comment for why a Denied
					// result can't just be ignored.
					OnItemPickupResult.Broadcast(static_cast<int32>(Result->target_id()),
						static_cast<int32>(Result->player_id()),
						Result->result() == ProtoType::Net::ResultCode::Ok);
				}
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyClaimResult:
			if (const auto* Result = Packet->payload_as_S2C_EnemyClaimResult())
			{
				OnEnemyClaimResult.Broadcast(static_cast<int32>(Result->enemy_id()), Result->granted());
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyState:
			if (const auto* State = Packet->payload_as_S2C_EnemyState())
			{
				const auto* Pos = State->position();
				const auto* Look = State->look();
				OnEnemyState.Broadcast(
					static_cast<int32>(State->enemy_id()),
					Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector,
					Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator,
					State->health(),
					State->is_dead());
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyOwnerLeft:
			if (const auto* Left = Packet->payload_as_S2C_EnemyOwnerLeft())
			{
				OnEnemyOwnerLeft.Broadcast(static_cast<int32>(Left->enemy_id()));
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyDamage:
			if (const auto* Damage = Packet->payload_as_S2C_EnemyDamage())
			{
				OnEnemyDamage.Broadcast(static_cast<int32>(Damage->enemy_id()), Damage->damage());
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyAttackResult:
			// Unicast by the server specifically to the target -- no
			// target_player_id filter needed, receiving this at all means
			// it's for this client's own local player.
			if (const auto* Attack = Packet->payload_as_S2C_EnemyAttackResult())
			{
				OnEnemyAttackPlayer.Broadcast(static_cast<int32>(Attack->enemy_id()), Attack->damage());
			}
			break;

		case ProtoType::Net::Payload::S2C_EnemyAttackBroadcast:
			if (const auto* AttackBroadcast = Packet->payload_as_S2C_EnemyAttackBroadcast())
			{
				OnEnemyAttackBroadcast.Broadcast(static_cast<int32>(AttackBroadcast->enemy_id()));
			}
			break;

		case ProtoType::Net::Payload::S2C_CompanionMoveState:
			if (const auto* State = Packet->payload_as_S2C_CompanionMoveState())
			{
				if (State->owner_id() != LocalPlayerId)
				{
					const auto* Pos = State->position();
					const auto* Look = State->look();
					UpdateRemoteCompanion(
						State->owner_id(),
						Pos ? FVector(Pos->x(), Pos->y(), Pos->z()) : FVector::ZeroVector,
						Look ? FRotator(Look->pitch(), Look->yaw(), Look->roll()) : FRotator::ZeroRotator,
						State->health(), State->is_dead(),
						State->weapon_type(), State->is_aiming(), State->aim_pitch());
				}
			}
			break;

		default:
			break;
	}
}

/*-------------------
 원격 플레이어 관리
-------------------*/
void UProtoNetClientSubsystem::UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation, bool bSprinting, bool bAiming)
{
	const int32 Key = static_cast<int32>(PlayerId);

	RemoteTargetLocation.Add(Key, Location);
	RemoteTargetRotation.Add(Key, Rotation);

	if (AActor** Existing = RemotePlayers.Find(Key))
	{
		if (IsValid(*Existing))
		{
			// Don't snap: TickRemotePlayers() walks it to the new target so
			// CharacterMovementComponent produces real walk/run animation.
			if (AProtoCharacter* RemoteCharacter = Cast<AProtoCharacter>(*Existing))
			{
				RemoteCharacter->GetCharacterMovement()->MaxWalkSpeed =
					bSprinting ? RemoteCharacter->SprintWalkSpeed : RemoteCharacter->BaseWalkSpeed;
				RemoteCharacter->SetRemoteAiming(bAiming, Rotation.Pitch);
			}
			return;
		}
		RemotePlayers.Remove(Key);
	}

	UWorld* World = GetWorld();
	if (!World)
		return;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* NewRemote = nullptr;
	if (RemoteCharacterClass)
	{
		// No controller assigned, so IsLocallyControlled() guards in
		// AProtoCharacter treat this as a remote spawn.
		AProtoCharacter* RemoteCharacter = World->SpawnActor<AProtoCharacter>(RemoteCharacterClass, Location, Rotation, SpawnParams);
		if (RemoteCharacter)
		{
			// CharacterMovementComponent aborts all movement and zeroes
			// velocity/acceleration for a Controller-less character unless
			// this is set (see bRunPhysicsWithNoController's doc comment) --
			// without it, TickRemotePlayers()'s AddMovementInput calls would
			// still be silently discarded even with bForce=true.
			if (UCharacterMovementComponent* MovementComponent = RemoteCharacter->GetCharacterMovement())
			{
				MovementComponent->bRunPhysicsWithNoController = true;

				// ACharacter::PostInitializeComponents() is what normally
				// bootstraps MovementMode out of MOVE_None into MOVE_Walking,
				// but only when bRunPhysicsWithNoController is *already* true
				// at that point (see its source: "if (CharacterMovement &&
				// CharacterMovement->bRunPhysicsWithNoController)"). That
				// runs synchronously inside SpawnActor() above, before we
				// ever get a chance to set the flag -- so MovementMode is
				// left stuck at MOVE_None forever, and PerformMovement()
				// early-outs on MOVE_None regardless of bForce on
				// AddMovementInput. Confirmed via [MoveDiag] logs from a
				// live 2-client test: mode=0 (MOVE_None) and speed=0.0 the
				// entire time. Explicitly finish the bootstrap ourselves.
				MovementComponent->SetMovementMode(MOVE_Walking);

				UE_LOG(LogProtoNet, Log, TEXT("[MoveDiag] spawned AProtoCharacter for player %u, bRunPhysicsWithNoController=%d, MovementMode=%d"),
					PlayerId, MovementComponent->bRunPhysicsWithNoController, static_cast<int32>(MovementComponent->MovementMode));
			}
			else
			{
				UE_LOG(LogProtoNet, Warning, TEXT("[MoveDiag] spawned AProtoCharacter for player %u but GetCharacterMovement() is NULL"), PlayerId);
			}
		}
		else
		{
			UE_LOG(LogProtoNet, Warning, TEXT("[MoveDiag] SpawnActor<AProtoCharacter> FAILED for player %u (class was valid)"), PlayerId);
		}
		NewRemote = RemoteCharacter;
	}
	else
	{
		UE_LOG(LogProtoNet, Warning, TEXT("[MoveDiag] RemoteCharacterClass is NULL -- BP_ProtoCharacter FClassFinder failed to resolve, falling back to placeholder actor for player %u"), PlayerId);
	}
	if (!NewRemote)
	{
		// Fallback placeholder if the character Blueprint couldn't be loaded.
		NewRemote = World->SpawnActor<AProtoRemotePlayer>(Location, Rotation, SpawnParams);
	}

	if (NewRemote)
	{
		RemotePlayers.Add(Key, NewRemote);
		UE_LOG(LogProtoNet, Log, TEXT("Spawned remote player %u"), PlayerId);
	}
}

void UProtoNetClientSubsystem::RemoveRemotePlayer(uint32 PlayerId)
{
	const int32 Key = static_cast<int32>(PlayerId);

	if (AActor** Existing = RemotePlayers.Find(Key))
	{
		if (IsValid(*Existing))
		{
			(*Existing)->Destroy();
		}
		RemotePlayers.Remove(Key);
	}
	RemoteTargetLocation.Remove(Key);
	RemoteTargetRotation.Remove(Key);

	UE_LOG(LogProtoNet, Log, TEXT("Removed remote player %u"), PlayerId);
}

void UProtoNetClientSubsystem::UpdateRemoteCompanion(uint32 OwnerId, const FVector& Location, const FRotator& Rotation, float Health, bool bIsDead,
	uint8 WeaponType, bool bIsAiming, float AimPitch)
{
	// See this function's header comment for why negated keys share the
	// same maps as real remote players instead of a separate set.
	const int32 Key = -static_cast<int32>(OwnerId);

	RemoteTargetLocation.Add(Key, Location);
	RemoteTargetRotation.Add(Key, Rotation);

	if (AActor** Existing = RemotePlayers.Find(Key))
	{
		if (IsValid(*Existing))
		{
			if (ACompanionNPC* ExistingCompanion = Cast<ACompanionNPC>(*Existing))
			{
				ExistingCompanion->MirroredHealth = Health;
				ExistingCompanion->bIsMirroredDead = bIsDead;
				ExistingCompanion->MirroredWeaponType = static_cast<EWeaponType>(WeaponType);
				ExistingCompanion->bMirroredIsAiming = bIsAiming;
				ExistingCompanion->MirroredAimPitch = AimPitch;
				return;
			}

			// Existing entry is the AProtoRemotePlayer fallback placeholder
			// (spawned below when RemoteCompanionClass was still null the
			// first time this owner's companion was ever seen -- e.g. this
			// client hadn't finished its own SpawnCompanion()/
			// SetRemoteCompanionClass() yet when the very first
			// C2S_CompanionMoveInput from a companion that joined later
			// arrived). Previously this just returned here forever, leaving
			// this owner's companion permanently stuck as an invisible/
			// unrecognizable placeholder even once RemoteCompanionClass
			// became valid moments later -- this was the "먼저 들어온
			// 플레이어가 나중에 들어온 플레이어의 컴패니언이 안 보임" bug.
			// Destroy the placeholder and fall through to spawn a real one
			// instead, same as if this were the first update ever.
			(*Existing)->Destroy();
			RemotePlayers.Remove(Key);
		}
		else
		{
			RemotePlayers.Remove(Key);
		}
	}

	UWorld* World = GetWorld();
	if (!World)
		return;

	AActor* NewRemote = nullptr;
	if (RemoteCompanionClass)
	{
		// Deferred so MarkAsRemotePuppet() runs before
		// PostInitializeComponents()/BeginPlay() -- see its declaration for
		// why that ordering matters (stops the AI controller from ever
		// auto-possessing, and the mic/LLM/AI/combat pipeline from ever
		// starting). Same SpawnActorDeferred+FinishSpawning idiom as
		// AEnemyBase::SpawnLoot().
		const FTransform SpawnTransform(Rotation, Location);
		ACompanionNPC* RemoteCompanion = World->SpawnActorDeferred<ACompanionNPC>(RemoteCompanionClass, SpawnTransform,
			nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (RemoteCompanion)
		{
			RemoteCompanion->MarkAsRemotePuppet();
			RemoteCompanion->MirroredHealth = Health;
			RemoteCompanion->bIsMirroredDead = bIsDead;
			RemoteCompanion->MirroredWeaponType = static_cast<EWeaponType>(WeaponType);
			RemoteCompanion->bMirroredIsAiming = bIsAiming;
			RemoteCompanion->MirroredAimPitch = AimPitch;
			RemoteCompanion->FinishSpawning(SpawnTransform);

			// Same bootstrap UpdateRemotePlayer() above needs and why --
			// a Controller-less Character's CharacterMovementComponent
			// otherwise stays stuck at MOVE_None forever.
			if (UCharacterMovementComponent* MovementComponent = RemoteCompanion->GetCharacterMovement())
			{
				MovementComponent->bRunPhysicsWithNoController = true;
				MovementComponent->SetMovementMode(MOVE_Walking);
			}
		}
		NewRemote = RemoteCompanion;
	}

	if (!NewRemote)
	{
		// Fallback placeholder if BP_CompanionNPC hasn't been resolved yet
		// (see RemoteCompanionClass's declaration).
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AProtoRemotePlayer* Placeholder = World->SpawnActor<AProtoRemotePlayer>(Location, Rotation, SpawnParams))
		{
			Placeholder->PlayerId = OwnerId;
			NewRemote = Placeholder;
		}
	}

	if (NewRemote)
	{
		RemotePlayers.Add(Key, NewRemote);
		UE_LOG(LogProtoNet, Log, TEXT("Spawned remote companion for owner %u (%s)"), OwnerId, *NewRemote->GetClass()->GetName());
	}
}

void UProtoNetClientSubsystem::RemoveRemoteCompanion(uint32 OwnerId)
{
	const int32 Key = -static_cast<int32>(OwnerId);

	if (AActor** Existing = RemotePlayers.Find(Key))
	{
		if (IsValid(*Existing))
		{
			(*Existing)->Destroy();
		}
		RemotePlayers.Remove(Key);
	}
	RemoteTargetLocation.Remove(Key);
	RemoteTargetRotation.Remove(Key);
}

void UProtoNetClientSubsystem::TickRemotePlayers(float DeltaTime)
{
	constexpr float ArrivalToleranceCm = 5.0f;

	// [MoveDiag] throttled diagnostic logging -- prints actor/target/velocity
	// once every ~0.5s per remote player instead of every tick, so a live
	// 2-player test can be diagnosed from the log file afterward.
	static float MoveDiagLogTimer = 0.0f;
	MoveDiagLogTimer -= DeltaTime;
	const bool bMoveDiagShouldLog = MoveDiagLogTimer <= 0.0f;
	if (bMoveDiagShouldLog)
	{
		MoveDiagLogTimer = 0.5f;
	}

	for (const auto& Pair : RemotePlayers)
	{
		AActor* RemoteActor = Pair.Value;
		if (!IsValid(RemoteActor))
			continue;

		// A dead remote companion should hold its last position, not keep
		// walking toward wherever its owner's (also-frozen, per
		// UCompanionAIComponent::TickComponent) real companion last
		// reported -- see MirroredHealth/bIsMirroredDead's comment.
		if (const ACompanionNPC* Companion = Cast<ACompanionNPC>(RemoteActor); Companion && Companion->bIsMirroredDead)
			continue;

		const FVector* TargetLocation = RemoteTargetLocation.Find(Pair.Key);
		if (!TargetLocation)
			continue;
		const FRotator* TargetRotation = RemoteTargetRotation.Find(Pair.Key);

		// ACharacter (not just AProtoCharacter) so this also smooths remote
		// AI companion puppets (see UpdateRemoteCompanion) -- none of the
		// calls below are AProtoCharacter-specific, they're all plain
		// ACharacter/APawn API.
		if (ACharacter* RemoteCharacter = Cast<ACharacter>(RemoteActor))
		{
			// Walk toward the target so CharacterMovementComponent produces
			// real velocity for the walk/run animation blend.
			// TargetRotation is the sender's full camera/control rotation
			// (Pitch/Yaw/Roll) -- the local player only ever turns their own
			// body with Yaw (bUseControllerRotationPitch/Roll are false in
			// the constructor), so applying Pitch/Roll here too would pitch
			// or roll the remote body to match the sender's look-up/down,
			// which reads as the whole mesh tipping toward the camera. Only
			// Yaw drives the actor's (and therefore the mesh's) rotation.
			// Smooth turn instead of snapping straight to the latest sample --
			// position updates arrive throttled (NetSyncInterval-ish, ~150ms
			// apart), so a hard SetActorRotation() every time one lands
			// visibly whips the body's facing around in discrete steps. See
			// AEnemyBase::MirroredRotationInterpSpeed for the same fix
			// applied to enemies (identical root cause, reported jerky in
			// the same live multiplayer test). FMath::RInterpTo takes the
			// shortest path around, so this doesn't spin the long way when
			// Yaw wraps.
			if (TargetRotation)
			{
				const FRotator NewRotation = FMath::RInterpTo(RemoteCharacter->GetActorRotation(),
					FRotator(0.0f, TargetRotation->Yaw, 0.0f), DeltaTime, /*InterpSpeed=*/10.0f);
				RemoteCharacter->SetActorRotation(NewRotation);
			}

			FVector ToTarget = *TargetLocation - RemoteCharacter->GetActorLocation();
			ToTarget.Z = 0.0f; // horizontal input only; let gravity/step-up handle height

			if (bMoveDiagShouldLog)
			{
				const UCharacterMovementComponent* MoveComp = RemoteCharacter->GetCharacterMovement();
				UE_LOG(LogProtoNet, Log, TEXT("[MoveDiag] id=%d actorLoc=%s target=%s dist=%.1f speed=%.1f mode=%d hasController=%d"),
					Pair.Key, *RemoteCharacter->GetActorLocation().ToString(), *TargetLocation->ToString(),
					ToTarget.Size(), RemoteCharacter->GetVelocity().Size(),
					MoveComp ? static_cast<int32>(MoveComp->MovementMode) : -1,
					RemoteCharacter->GetController() != nullptr);
			}

			if (ToTarget.SizeSquared() > FMath::Square(ArrivalToleranceCm))
			{
				// bForce=true: AddMovementInput() normally routes through
				// Controller->Internal_AddMovementInput() and is a no-op
				// without one (see APawn::AddMovementInput). Remote-spawned
				// characters have no Controller (see the spawn comment
				// above), so without bForce this silently dropped every
				// frame and the character never left its spawn point.
				RemoteCharacter->AddMovementInput(ToTarget.GetSafeNormal(), 1.0f, /*bForce=*/true);
			}
		}
		else
		{
			// Fallback placeholder: no movement component, so just snap it.
			RemoteActor->SetActorLocationAndRotation(
				*TargetLocation, TargetRotation ? *TargetRotation : RemoteActor->GetActorRotation());
		}
	}
}

void UProtoNetClientSubsystem::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

/*-------------------
 FTickableGameObject 오버라이드
-------------------*/
void UProtoNetClientSubsystem::Tick(float DeltaTime)
{
	TArray<uint8> Packet;
	while (ReceivedPackets.Dequeue(Packet))
	{
		HandleIncomingPacket(Packet);
		OnPacketReceived.Broadcast(Packet);
	}

	// Same framed Packet bytes, same HandleIncomingPacket -- payload_type()
	// alone decides behavior, so nothing here needs to know these arrived
	// on the Game connection specifically (see 매칭 서버 설계, step 5).
	while (GameReceivedPackets.Dequeue(Packet))
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

	FString GameReason;
	if (GameDisconnectReasons.Dequeue(GameReason))
	{
		// The Login connection (and therefore the account session) is
		// unaffected -- only the raid/gameplay connection dropped. See
		// OnDisconnectedFromGameServer's comment.
		UE_LOG(LogProtoNet, Warning, TEXT("Disconnected from Game server: %s"), *GameReason);
		DisconnectFromGameServer();
		OnDisconnectedFromGameServer.Broadcast(GameReason);
	}

	TickRemotePlayers(DeltaTime);
}

TStatId UProtoNetClientSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UProtoNetClientSubsystem, STATGROUP_Tickables);
}

bool UProtoNetClientSubsystem::IsTickable() const
{
	return true;
}
