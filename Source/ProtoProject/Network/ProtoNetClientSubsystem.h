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
// connection speaking the same Protocol (FlatBuffers, size-prefixed "PTPK").
// A GameInstanceSubsystem so it's reachable from Blueprint anywhere.
UCLASS()
class PROTOPROJECT_API UProtoNetClientSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/*-------------------
	 생성/소멸
	-------------------*/
	UProtoNetClientSubsystem();
	// Defined in the .cpp (where FProtoNetReceiveWorker is complete) so
	// TUniquePtr's deleter isn't instantiated against an incomplete type.
	virtual ~UProtoNetClientSubsystem() override;
	// UHT's auto-generated FVTableHelper ctor needs this declared+defined
	// the same way, for the same reason.
	UProtoNetClientSubsystem(FVTableHelper& Helper);

	//~ UGameInstanceSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UGameInstanceSubsystem

	/*-------------------
	 접속 관리
	-------------------*/
	// Connects to the echo server. Defaults to the server running on this
	// same machine (127.0.0.1:7777).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool Connect(const FString& ServerIp = TEXT("127.0.0.1"), int32 ServerPort = 7777);

	// Shows an on-screen "enter server IP" prompt and connects + logs in once
	// submitted. No-op if already connected or already shown.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void ShowConnectPrompt();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool IsConnected() const;

	/*-------------------
	 패킷 송신
	-------------------*/
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

	// Weapon and item pickups both use InteractType::Loot; the protocol has
	// no separate type to tell them apart.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendInteractLoot(int32 TargetId);

	// Reports the local player's position/facing so others can see them move.
	// Called periodically (throttled), not meant to be spammed every frame.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendMoveInput(FVector Position, FRotator Look, int32 Flags = 0);

	// Lets other clients mirror the reload motion. WeaponType (EWeaponType)
	// is reused as the "slot" field so the receiver knows which section to play.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendWeaponReload(uint8 WeaponType);

	// Lets other clients show the right held weapon (None = storing it).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendWeaponEquip(uint8 WeaponType);

	/*-------------------
	 상태 조회 / 델리게이트
	-------------------*/
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

	/*-------------------
	 수신 패킷 처리
	-------------------*/
	// Decodes one framed packet and updates local state or the matching
	// remote player actor, depending on payload type.
	void HandleIncomingPacket(const TArray<uint8>& PacketBytes);

	/*-------------------
	 원격 플레이어 관리
	-------------------*/
	// Spawns the remote player on first sight and records its target
	// transform; TickRemotePlayers() walks it there every frame so
	// CharacterMovementComponent produces real walk/run animation.
	void UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation, bool bSprinting = false);
	void TickRemotePlayers(float DeltaTime);

	// Despawns a remote player's actor and clears its tracking entries, on
	// receiving S2C_PlayerLeft (the server broadcasts this when a session
	// disconnects). No-op if PlayerId isn't currently tracked.
	void RemoveRemotePlayer(uint32 PlayerId);

	// Same Blueprint the local player uses, so remote players look real.
	// Falls back to the AProtoRemotePlayer placeholder if it fails to load.
	TSubclassOf<AProtoCharacter> RemoteCharacterClass;

	/*-------------------
	 접속 프롬프트
	-------------------*/
	void HandleConnectPromptSubmitted(const FString& ServerIp);
	void HideConnectPrompt();

	TSharedPtr<SProtoConnectPrompt> ConnectPromptWidget;

	/*-------------------
	 멤버 변수
	-------------------*/
	FSocket* Socket = nullptr;
	TUniquePtr<FProtoNetReceiveWorker> Worker;
	FRunnableThread* WorkerThread = nullptr;
	FCriticalSection SendLock;
	uint32 NextSeq = 1;
	uint32 LocalPlayerId = 0;

	// Set on a successful Connect(); ShowConnectPrompt() prefills this so an
	// unexpected disconnect's reconnect prompt just needs Enter, not retyping.
	FString LastServerIp;

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
