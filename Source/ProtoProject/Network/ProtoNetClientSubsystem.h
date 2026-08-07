#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "ProtoNetClientSubsystem.generated.h"

class AActor;
class FSocket;
class FRunnableThread;
class FProtoNetReceiveWorker;
class AProtoRemotePlayer;
class AProtoCharacter;
class SProtoConnectPrompt;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnPacketReceived, const TArray<uint8>&, PacketBytes);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FProtoOnConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnDisconnected, const FString&, Reason);

// Client-side counterpart to the WOP_SERVER RIO echo server: a plain TCP
// connection (blocking recv on a background thread) that speaks the same
// Protocol (FlatBuffers, size-prefixed "PTPK" packets). Runs as a
// GameInstanceSubsystem so it is reachable from Blueprint without needing to
// place anything in a level.
UCLASS()
class PROTOPROJECT_API UProtoNetClientSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UProtoNetClientSubsystem();
	// Declared (and defined in the .cpp, where FProtoNetReceiveWorker's full
	// definition is visible) so the compiler doesn't try to instantiate
	// TUniquePtr<FProtoNetReceiveWorker>'s deleter against an incomplete type.
	virtual ~UProtoNetClientSubsystem() override;
	// UHT also auto-generates an FVTableHelper constructor in its .gen.cpp,
	// which only sees this header (not FProtoNetReceiveWorker's full
	// definition) unless it is declared here and defined in the .cpp too.
	UProtoNetClientSubsystem(FVTableHelper& Helper);

	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UGameInstanceSubsystem

	// Connects to the echo server. Defaults to the server running on this
	// same machine (127.0.0.1:7777).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool Connect(const FString& ServerIp = TEXT("127.0.0.1"), int32 ServerPort = 7777);

	// Shows an on-screen "enter server IP" prompt (defaults to -ServerIP=
	// from the command line, or 127.0.0.1) and connects + logs in once the
	// player submits it. Called from AProtoCharacter::BeginPlay() for the
	// locally-controlled player. No-op if already connected or already shown.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void ShowConnectPrompt();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool IsConnected() const;

	// Sends an already-framed (4-byte size-prefixed) Protocol Packet buffer.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendPacketBytes(const TArray<uint8>& PacketBytes);

	// Convenience call for testing the connection: builds and sends a
	// C2S_Login packet with the given fields.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendLoginTest(const FString& AuthToken, const FString& ClientVersion);

	// Called from AAK47::Fire() when a shot is fired.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendAttackFire(FVector Origin, FVector Direction, uint8 WeaponSlot = 0);

	// Called from ADropItem::OnInteract_Implementation() when a weapon or
	// item is picked up (the protocol has no separate "weapon vs item"
	// interact type, so both use InteractType::Loot).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendInteractLoot(int32 TargetId);

	// Reports the local player's current world position/facing so other
	// connected clients can see this player move. Called periodically from
	// AProtoCharacter::Tick() (throttled), not meant to be spammed every frame.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendMoveInput(FVector Position, FRotator Look, int32 Flags = 0);

	// Called from AProtoCharacter::ReloadWeapon() so other clients can mirror
	// the reload motion. WeaponType is EWeaponType cast to uint8 (reused as
	// the "slot" field so the receiver knows which reload section to play).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendWeaponReload(uint8 WeaponType);

	// Called from AProtoCharacter::BeginWeaponSwap() so other clients see
	// this player holding (or storing, for EWeaponType::None) the right
	// weapon. WeaponType is EWeaponType cast to uint8.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendWeaponEquip(uint8 WeaponType);

	// This client's own player id, assigned by the server after login via
	// S2C_LoginSuccess. 0 until then.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	int32 GetLocalPlayerId() const { return static_cast<int32>(LocalPlayerId); }

	// Fired on the game thread for every complete packet the server sends
	// back (raw, still-framed bytes; PacketBytes[4:] is the FlatBuffers Packet).
	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnPacketReceived OnPacketReceived;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnConnected OnConnected;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnDisconnected OnDisconnected;

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	//~ End FTickableGameObject

private:
	friend class FProtoNetReceiveWorker;

	// Decodes one complete framed packet and, for the multiplayer-relevant
	// types (login success / player join / move state), updates local state
	// or spawns/moves the matching remote player actor.
	void HandleIncomingPacket(const TArray<uint8>& PacketBytes);

	// Spawns the remote player on first sight and/or records where it should
	// be heading; TickRemotePlayers() is what actually walks it there every
	// frame so CharacterMovementComponent produces real velocity for the
	// walk/run animation blend (a straight SetActorLocation teleport would
	// leave the character in its idle pose).
	void UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation, bool bSprinting = false);
	void TickRemotePlayers(float DeltaTime);

	// The same Blueprint the local player uses (BP_ProtoCharacter), loaded in
	// the constructor, so remote players look like real characters instead of
	// the AProtoRemotePlayer placeholder. Falls back to the placeholder if
	// this can't be loaded (e.g. the asset was moved/renamed).
	TSubclassOf<AProtoCharacter> RemoteCharacterClass;

	void HandleConnectPromptSubmitted(const FString& ServerIp);
	void HideConnectPrompt();

	TSharedPtr<SProtoConnectPrompt> ConnectPromptWidget;

	FSocket* Socket = nullptr;
	TUniquePtr<FProtoNetReceiveWorker> Worker;
	FRunnableThread* WorkerThread = nullptr;
	FCriticalSection SendLock;
	uint32 NextSeq = 1;
	uint32 LocalPlayerId = 0;

	UPROPERTY()
	TMap<int32, AActor*> RemotePlayers;

	// Latest reported transform per remote player; TickRemotePlayers() walks
	// each character toward its entry every frame.
	TMap<int32, FVector> RemoteTargetLocation;
	TMap<int32, FRotator> RemoteTargetRotation;

	// Filled by the worker thread, drained on the game thread in Tick().
	TQueue<TArray<uint8>, EQueueMode::Mpsc> ReceivedPackets;
	TQueue<FString, EQueueMode::Mpsc> DisconnectReasons;
};
