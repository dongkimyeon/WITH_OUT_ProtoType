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
class ACompanionNPC;

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

// One placed item in the grid inventory, as sent to/from the server. ItemId
// is the item Data Asset's own object name (e.g. "DA_Item_AK47") -- see
// AProtoCharacter::ResolveItemDataByName for how it's turned back into a
// UItemDataBase* on restore.
USTRUCT(BlueprintType)
struct FProtoInventoryItemEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 GridX = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 GridY = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	bool bRotated = false;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 StackCount = 1;
};

// Fired once after EVERY S2C_LoginSuccess (unlike OnProgressRestored, not
// gated on has_saved_progress) -- Items is empty if the account has no
// saved inventory. Listeners should treat this as "here is the complete,
// authoritative state," always replacing whatever's currently shown rather
// than only adding, so a different account logged into by the same running
// client can't leave stale items behind.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnInventoryRestored, const TArray<FProtoInventoryItemEntry>&, Items);

// Fired whenever the server answers a container loot roll (see
// SendContainerLootRoll) with the authoritative contents for ContainerId --
// either this client's own roll (if it was first) or another client's
// earlier one. AItemContainerBase-derived actors bind this in BeginPlay and
// filter by their own GetContainerId(), so nothing needs a central registry
// of which container is waiting for which reply.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnContainerLootState, int32, ContainerId, const TArray<FProtoInventoryItemEntry>&, Items);

// Fired on S2C_EnemyClaimResult, answering a SendEnemyClaimRequest for
// EnemyId. bGranted true means this client is now the one running that
// enemy's AI locally and should start calling SendEnemyState for it.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnEnemyClaimResult, int32, EnemyId, bool, bGranted);

// Fired on S2C_EnemyState -- the CURRENT owner's authoritative
// position/facing/health/dead state for EnemyId. Never fires for an enemy
// this client itself owns (the server never echoes C2S_EnemyState back to
// its sender).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FProtoOnEnemyState, int32, EnemyId, FVector, Position, FRotator, Look, float, Health, bool, bIsDead);

// Fired on S2C_EnemyOwnerLeft -- EnemyId's driving session disconnected.
// The enemy itself still exists; a non-owner should try SendEnemyClaimRequest
// again so it doesn't stay frozen forever.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnEnemyOwnerLeft, int32, EnemyId);

// Fired on S2C_EnemyDamage -- only ever received by the current owner of
// EnemyId (see that message's schema comment), relaying a hit some other
// client's local trace landed on it.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnEnemyDamage, int32, EnemyId, float, Damage);

// Fired on S2C_EnemyAttackResult -- a SERVER-DRIVEN enemy_id (see
// SendEnemyRegister) landed a melee hit on the LOCAL player. Unicast by the
// server, so every receipt of this is meant for this client -- see that
// message's schema comment for the trust model (server decides if/when/how
// much, this client applies it to its own health).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnEnemyAttackPlayer, int32, EnemyId, float, Damage);

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

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void Disconnect();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool IsConnected() const;

	/*-------------------
	 복원 데이터 (늦게 구독하는 리스너용)
	-------------------*/
	// S2C_LoginSuccess can arrive (and OnProgressRestored/OnInventoryRestored
	// can fire) while still on TitleLevel, before any AProtoCharacter exists
	// to bind those delegates -- the character only spawns (and binds, in its
	// own BeginPlay) after HandleLoginSucceeded's OpenLevelBySoftObjectPtr()
	// finishes the level transition. A dynamic multicast delegate doesn't
	// replay past broadcasts to a listener that binds afterward, so that
	// character would silently never see its restore. These Consume*
	// functions let BeginPlay pull whatever was cached by the login that
	// already happened, once, in addition to the (still useful, for the old
	// in-game Slate popup where the character already exists) broadcasts.
	// Returns false (and leaves the outputs untouched) if there's nothing
	// pending or it was already consumed.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConsumePendingProgressRestore(FVector& OutPosition, FRotator& OutLook, uint8& OutWeaponType);

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConsumePendingInventoryRestore(TArray<FProtoInventoryItemEntry>& OutItems);

	// Same pending-restore cache as above, but seeded directly from local
	// state instead of a server round-trip -- called by
	// AProtoCharacter::EndPlay(LevelTransition) so weapon/inventory survive
	// moving between levels within the same login session (Test ->
	// Single/Multi via LevelChanger and back). Those transitions don't
	// involve a fresh login, so there's no S2C_LoginSuccess to populate the
	// cache the normal way -- without this, every level change silently
	// dropped whatever the player was holding.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void CacheStateForLevelTransition(FVector Position, FRotator Look, uint8 WeaponType, const TArray<FProtoInventoryItemEntry>& InventoryItems);

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

	// Persists the account's full current grid inventory contents (not a
	// diff -- see C2S_SaveInventory's schema comment). Not gated by
	// SetMultiplayerVisualsEnabled: this is account persistence, not
	// something other players see, so it works the same in Single and Multi.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSaveInventory(const TArray<FProtoInventoryItemEntry>& Items);

	// See C2S_SetVisible's schema comment -- called by
	// SetMultiplayerVisualsEnabled(false) so other clients despawn this
	// player instead of freezing it in place as a "ghost" (this session
	// stays connected the whole time, it just stops sending move updates).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSetVisible(bool bVisible);

	// Reports the local roll an AItemContainerBase-derived actor generated
	// for itself on BeginPlay (see ALootContainer::SeedContents). The
	// server answers via OnContainerLootState with the authoritative
	// contents -- either this roll, if it's the first for ContainerId, or
	// an earlier client's -- so every client agrees on what's in the box.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendContainerLootRoll(int32 ContainerId, const TArray<FProtoInventoryItemEntry>& Items);

	// Reports the local AI companion's position/facing, throttled (same
	// idea as SendMoveInput), so other clients can mirror it via a
	// placeholder actor (see UpdateRemoteCompanion). Not gated by
	// bMultiplayerVisualsEnabled for the SEND direction -- same reasoning
	// as SendSaveInventory, this is "what my companion is doing" regardless
	// of whether this client currently wants to see other players.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendCompanionMoveInput(FVector Position, FRotator Look);

	// Called once by the LOCAL player's own AProtoCharacter::SpawnCompanion()
	// right after it spawns its companion, so remote companion puppets (see
	// UpdateRemoteCompanion) can reuse the SAME already-safely-loaded
	// BP_CompanionNPC class -- deliberately NOT resolved here via
	// ConstructorHelpers::FClassFinder the way RemoteCharacterClass is:
	// BP_CompanionNPC's AnimBP references BP_ProtoCharacter right back, and a
	// synchronous load at the wrong moment during startup previously
	// deadlocked ("LoadingIsStuck" -- see AProtoCharacter's constructor
	// comment). Until the first local companion spawns, RemoteCompanionClass
	// stays null and remote companions fall back to the AProtoRemotePlayer
	// placeholder.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void SetRemoteCompanionClass(TSubclassOf<ACompanionNPC> InClass) { RemoteCompanionClass = InClass; }

	// Sent once, right after BeginPlay, by every placed AEnemyBase -- "I'm
	// ready to drive this enemy's AI if nobody already is." See
	// OnEnemyClaimResult for the answer.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyClaimRequest(int32 EnemyId);

	// Throttled, called only by whichever client owns EnemyId (i.e. only
	// after OnEnemyClaimResult granted it) -- see C2S_EnemyState's schema
	// comment.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyState(int32 EnemyId, FVector Position, FRotator Look, float Health, bool bIsDead);

	// Called by a NON-owning client whose own local hit detection landed a
	// shot on EnemyId, so the server can relay it to whichever client
	// actually owns that enemy's health. See C2S_EnemyDamage's schema comment.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyDamage(int32 EnemyId, float Damage);

	// Sent once, right after BeginPlay, by every placed AEnemyBase that
	// finds itself in a multiplayer-visible map -- unlike
	// SendEnemyClaimRequest, this doesn't ask permission: the SERVER drives
	// this enemy's AI from here on (see C2S_EnemyRegister's schema comment),
	// and the caller should switch straight to mirroring OnEnemyState
	// without waiting for a reply. AttackDamage/AttackCooldown are reported
	// too (not just move/range) so the server's own melee timing matches
	// this enemy's actual Blueprint-configured stats instead of a hardcoded
	// guess -- see OnEnemyAttackPlayer for the other half of this.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyRegister(int32 EnemyId, FVector Position, float Health, float MaxHealth, float MoveSpeed, float AttackRange, float AttackDamage, float AttackCooldown);

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
	FProtoOnInventoryRestored OnInventoryRestored;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnContainerLootState OnContainerLootState;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnEnemyClaimResult OnEnemyClaimResult;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnEnemyState OnEnemyState;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnEnemyOwnerLeft OnEnemyOwnerLeft;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnEnemyDamage OnEnemyDamage;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnEnemyAttackPlayer OnEnemyAttackPlayer;

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

	// Spawns/updates a visual mirror of another player's AI companion.
	// Shares RemotePlayers/RemoteTargetLocation/RemoteTargetRotation with
	// real players -- keyed by -(int32)OwnerId so the two id spaces can
	// never collide (real player ids are always positive). Spawns a REAL
	// ACompanionNPC (via RemoteCompanionClass, i.e. BP_CompanionNPC) marked
	// with MarkAsRemotePuppet() -- see that function's comment -- so it
	// shows the actual companion mesh and walks there through
	// CharacterMovementComponent (TickRemotePlayers()'s Cast<ACharacter>
	// branch smooths it the same way it does real players) without ever
	// running its own mic/LLM/AI/combat pipeline: this is someone ELSE's
	// companion, and spinning that up here would be both wasteful and
	// wrong (nothing should be feeding it commands but its own owner).
	// Falls back to the plain AProtoRemotePlayer placeholder if
	// RemoteCompanionClass hasn't been set yet (see SetRemoteCompanionClass).
	// Known gap: only position/facing are synced, not weapon/aim state, so
	// a remote companion's puppet always renders unarmed.
	void UpdateRemoteCompanion(uint32 OwnerId, const FVector& Location, const FRotator& Rotation);

	// Despawns a remote companion placeholder and clears its tracking
	// entries. Also called from RemoveRemotePlayer -- a player's companion
	// can't outlive them.
	void RemoveRemoteCompanion(uint32 OwnerId);

	// See SetMultiplayerVisualsEnabled(). Gates both the Send*() helpers
	// below and the remote-player-affecting cases in HandleIncomingPacket();
	// login/connection packets are unaffected.
	bool bMultiplayerVisualsEnabled = true;

	// Same Blueprint the local player uses, so remote players look real.
	// Falls back to the AProtoRemotePlayer placeholder if it fails to load.
	TSubclassOf<AProtoCharacter> RemoteCharacterClass;

	// See SetRemoteCompanionClass. Null until the local player's own
	// companion has spawned at least once this session.
	TSubclassOf<ACompanionNPC> RemoteCompanionClass;

	/*-------------------
	 멤버 변수
	-------------------*/
	FSocket* Socket = nullptr;
	TUniquePtr<FProtoNetReceiveWorker> Worker;
	FRunnableThread* WorkerThread = nullptr;
	FCriticalSection SendLock;
	uint32 NextSeq = 1;
	uint32 LocalPlayerId = 0;

	// Set on a successful Connect()/account login.
	FString LastServerIp;
	FString LastUsername;

	// See ConsumePendingProgressRestore/ConsumePendingInventoryRestore above.
	// Set on every S2C_LoginSuccess (alongside the broadcasts), cleared by
	// the corresponding Consume* call or by Disconnect() so a failed/aborted
	// connection can't leak stale data into the next login attempt.
	bool bHasPendingProgressRestore = false;
	FVector PendingRestorePosition = FVector::ZeroVector;
	FRotator PendingRestoreLook = FRotator::ZeroRotator;
	uint8 PendingRestoreWeaponType = 0;

	bool bHasPendingInventoryRestore = false;
	TArray<FProtoInventoryItemEntry> PendingRestoreInventory;

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
