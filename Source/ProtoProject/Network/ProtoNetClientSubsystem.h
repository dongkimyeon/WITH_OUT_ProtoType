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
// Fired once, right after S2C_LoginSuccess, only when the account had a
// saved PlayerProgress row. AProtoCharacter's locally-controlled instance
// binds this in BeginPlay() to move/re-equip itself to where it left off.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FProtoOnProgressRestored, FVector, Position, FRotator, Look, uint8, WeaponType);

// Mirrors ProtoType::Net::LoginFailReason 1:1 (raw flatbuffers enums aren't
// Blueprint-visible), for TitleLevel UI to switch on.
UENUM(BlueprintType)
enum class EProtoLoginFailReason : uint8
{
	InvalidToken,
	VersionMismatch,
	ServerFull,
	Banned,
	AlreadyLoggedIn,
	Unknown,
	AccountNotFound,	// Login: no account with that username.
	UsernameTaken,		// Register: an account with that username already exists.
};

// Fired on S2C_LoginSuccess, regardless of whether the login came from the
// Slate connect prompt or a TitleLevel UMG widget (ConnectAndLogin/ConnectAndRegister).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnLoginSucceeded, int32, PlayerId, bool, bHasSavedProgress);
// Fired on S2C_LoginFail, same as above.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnLoginFailed, EProtoLoginFailReason, Reason, const FString&, Message);

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
	 싱글/멀티 모드
	-------------------*/
	// Solo maps stay connected (so login/progress-save keeps working) but
	// stop broadcasting our moves/actions to other players and stop showing
	// theirs -- called from LevelChangeSelectWidget when picking Single vs
	// Multi. Defaults to true (this is the "test" level's always-on behavior).
	// Turning it off immediately despawns any remote players already visible.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void SetMultiplayerVisualsEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool IsMultiplayerVisualsEnabled() const { return bMultiplayerVisualsEnabled; }

	/*-------------------
	 패킷 송신
	-------------------*/
	// Sends an already-framed (4-byte size-prefixed) Protocol Packet buffer.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendPacketBytes(const TArray<uint8>& PacketBytes);

	// Convenience call for testing the connection: builds and sends a
	// C2S_Login packet with the given fields, no account credentials.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendLoginTest(const FString& AuthToken, const FString& ClientVersion);

	// Logs into a DB-backed account (bIsRegister=false, the default) or
	// creates a brand new one (bIsRegister=true). The server replies with
	// S2C_LoginSuccess (has_saved_progress tells you whether
	// OnProgressRestored will also fire) or S2C_LoginFail (AccountNotFound,
	// UsernameTaken, wrong password, ...).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendAccountLogin(const FString& Username, const FString& Password, bool bIsRegister = false);

	// Convenience for TitleLevel's LoginButton: connects if not already
	// connected, then logs in with an existing account. Fires OnLoginSucceeded
	// / OnLoginFailed (in addition to, not instead of, the Slate connect
	// prompt's own handling, which stays inert if that prompt isn't shown).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConnectAndLogin(const FString& ServerIp, const FString& Username, const FString& Password);

	// Convenience for TitleLevel's SignInButton: connects if not already
	// connected, then registers a brand new account. Fails with
	// UsernameTaken if the username already exists.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConnectAndRegister(const FString& ServerIp, const FString& Username, const FString& Password);

	// Called from AAK47::Fire() when a shot is fired.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendAttackFire(FVector Origin, FVector Direction, uint8 WeaponSlot = 0);

	// Weapon and item pickups both use InteractType::Loot; the protocol has
	// no separate type to tell them apart.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendInteractLoot(int32 TargetId);

	// Reports the local player's position/facing so others can see them move.
	// Called periodically (throttled), not meant to be spammed every frame.
	// Flags is a ProtoType::Net::MoveFlags bitmask -- see kMoveFlagSprint/
	// kMoveFlagADS below for the bits AProtoCharacter actually sets, so
	// PlayerContent code doesn't need to include the flatbuffers headers
	// just to build a Flags value.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendMoveInput(FVector Position, FRotator Look, int32 Flags = 0);

	static constexpr int32 kMoveFlagSprint = 1;
	static constexpr int32 kMoveFlagADS = 16;

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

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnProgressRestored OnProgressRestored;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnLoginSucceeded OnLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnLoginFailed OnLoginFailed;

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
	// Rotation.Pitch also doubles as the aim-offset pitch when bAiming is
	// true (see AProtoCharacter::SetRemoteAiming) -- it's the same Look the
	// sender's own camera used, no separate field needed.
	void UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation, bool bSprinting = false, bool bAiming = false);
	void TickRemotePlayers(float DeltaTime);

	// Despawns a remote player's actor and clears its tracking entries, on
	// receiving S2C_PlayerLeft (the server broadcasts this when a session
	// disconnects). No-op if PlayerId isn't currently tracked.
	void RemoveRemotePlayer(uint32 PlayerId);

	// Despawns every currently-tracked remote player at once. Used by
	// SetMultiplayerVisualsEnabled(false) and reuses the same cleanup
	// Disconnect() does.
	void RemoveAllRemotePlayers();

	// See SetMultiplayerVisualsEnabled(). Gates both the Send*() helpers
	// below and the remote-player-affecting cases in HandleIncomingPacket();
	// login/connection packets are unaffected.
	bool bMultiplayerVisualsEnabled = true;

	// Same Blueprint the local player uses, so remote players look real.
	// Falls back to the AProtoRemotePlayer placeholder if it fails to load.
	TSubclassOf<AProtoCharacter> RemoteCharacterClass;

	/*-------------------
	 접속 프롬프트
	-------------------*/
	void HandleConnectPromptSubmitted(const FString& ServerIp, const FString& Username, const FString& Password);
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

	// Set on a successful Connect()/account login; ShowConnectPrompt()
	// prefills these so an unexpected disconnect's reconnect prompt just
	// needs Enter (+ retyping the password, which is never remembered).
	FString LastServerIp;
	FString LastUsername;

	// True while we're waiting on S2C_LoginSuccess/S2C_LoginFail for an
	// account login submitted via the connect prompt, so HandleIncomingPacket
	// knows whether to update/hide the prompt on those replies.
	bool bAwaitingAccountLoginReply = false;

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
