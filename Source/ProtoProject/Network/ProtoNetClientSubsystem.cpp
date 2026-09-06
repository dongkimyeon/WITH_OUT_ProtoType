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
	Worker = MakeUnique<FProtoNetReceiveWorker>(Socket, this);
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

	// Nothing will tell us about these players again until we reconnect and
	// get a fresh roster -- despawn them now instead of leaving frozen ghosts.
	RemoveAllRemotePlayers();
	LocalPlayerId = 0;

	// A failed/aborted connection shouldn't leak this login's restore data
	// into whatever the next successful login turns out to be.
	bHasPendingProgressRestore = false;
	bHasPendingInventoryRestore = false;
	PendingRestoreInventory.Empty();
	PendingRestoreEquipment.Empty();
	PendingRestoreQuickSlots.Empty();
}

bool UProtoNetClientSubsystem::IsConnected() const
{
	return Socket != nullptr && Socket->GetConnectionState() == SCS_Connected;
}

bool UProtoNetClientSubsystem::ConsumePendingProgressRestore(FVector& OutPosition, FRotator& OutLook, uint8& OutWeaponType)
{
	if (!bHasPendingProgressRestore)
		return false;

	OutPosition = PendingRestorePosition;
	OutLook = PendingRestoreLook;
	OutWeaponType = PendingRestoreWeaponType;
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendPlayerDied()
{
	if (!bMultiplayerVisualsEnabled || !IsConnected())
		return false;

	flatbuffers::FlatBufferBuilder Fbb;
	auto Req = ProtoType::Net::CreateC2S_PlayerDied(Fbb);
	auto Packet = ProtoType::Net::CreatePacket(Fbb, ProtoType::Net::Payload::C2S_PlayerDied, Req.Union());
	ProtoType::Net::FinishSizePrefixedPacketBuffer(Fbb, Packet);

	TArray<uint8> Bytes;
	Bytes.Append(Fbb.GetBufferPointer(), static_cast<int32>(Fbb.GetSize()));
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
}

bool UProtoNetClientSubsystem::SendCompanionMoveInput(FVector Position, FRotator Look, float Health, bool bIsDead,
	uint8 WeaponType, bool bIsAiming, float AimPitch)
{
	if (!IsConnected())
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
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
	return SendPacketBytes(Bytes);
}

void UProtoNetClientSubsystem::CacheStateForLevelTransition(FVector Position, FRotator Look, uint8 WeaponType,
	const TArray<FProtoInventoryItemEntry>& InventoryItems, const TArray<FProtoEquipmentEntry>& Equipment,
	const TArray<FProtoQuickSlotEntry>& QuickSlots)
{
	bHasPendingProgressRestore = true;
	PendingRestorePosition = Position;
	PendingRestoreLook = Look;
	PendingRestoreWeaponType = WeaponType;

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
						RemoteCharacter->HandleRemotePlayerDied();
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
			}
			return;
		}
		RemotePlayers.Remove(Key);
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

	FString Reason;
	if (DisconnectReasons.Dequeue(Reason))
	{
		UE_LOG(LogProtoNet, Warning, TEXT("Disconnected: %s"), *Reason);
		Disconnect();
		OnDisconnected.Broadcast(Reason);
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
