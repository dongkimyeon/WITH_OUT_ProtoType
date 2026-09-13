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
// binds this in BeginPlay() to re-equip itself to where it left off.
// Position is intentionally NOT restored (see HandleProgressRestored) --
// every level's own PlayerStart is used instead, always. A level transition
// (fresh DB login, Single/Multi toggle, or an extraction back to
// SafePlaceLevel) can land a saved coordinate in a completely different
// level's geometry -- inside a wall, off the nav mesh, in the void -- with
// no way to tell whether it's actually safe to reapply. This was the
// "강제종료한 플레이어의 위치가 이상한곳으로 고정되는 문제" bug (a
// force-quit mid-raid saves a raid coordinate that a later login misapplies
// inside SafePlaceLevel); rather than track which level each save belongs to
// and reason about which transitions are "safe", always spawning at
// PlayerStart sidesteps the whole problem -- including the same risk on a
// normal extraction, which this delegate's Position alone couldn't have
// protected against anyway.
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
// Slate connect prompt or a TitleLevel UMG widget (ConnectAndLogin/ConnectAndRegister)
// OR a successful ConnectToGameServerAndJoin (see that function's comment --
// C2S_JoinMatch's reply is the SAME S2C_LoginSuccess message, restoring the
// SAME account's saved progress, just arriving over the Game connection
// instead of the Login one).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnLoginSucceeded, int32, PlayerId, bool, bHasSavedProgress);
// Fired on S2C_LoginFail, same as above.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnLoginFailed, EProtoLoginFailReason, Reason, const FString&, Message);

// Mirrors ProtoType::Net::JoinMatchFailReason 1:1, same reasoning as
// EProtoLoginFailReason above.
UENUM(BlueprintType)
enum class EProtoJoinMatchFailReason : uint8
{
	InvalidOrExpiredTicket,
};

// Login/Game server split (매칭 서버 설계, step 5): fired on S2C_MatchTicket,
// the reply to RequestMatch(). GameServerHost/GameServerPort say where to
// open the second connection (GameServerHost empty means "same host you're
// already talking to" -- see that field's schema comment); Ticket is what
// ConnectToGameServerAndJoin needs. ExpiresAtUnixMs is informational only
// (the server is the one that actually enforces it).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FProtoOnMatchTicketReceived, const FString&, GameServerHost, int32, GameServerPort, const FString&, Ticket, int64, ExpiresAtUnixMs);

// Fired on S2C_JoinMatchFail -- ConnectToGameServerAndJoin's ticket was
// invalid, already used, or expired. Request a fresh one via RequestMatch().
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnJoinMatchFailed, EProtoJoinMatchFailReason, Reason);

// Fired when the GAME connection specifically drops (see
// DisconnectFromGameServer/ConnectToGameServerAndJoin) -- distinct from
// OnDisconnected, which is about the Login connection and generally means
// "go back to the title screen". Losing the Game connection mid-raid is
// recoverable: the Login connection (and therefore the account session)
// stays up, so the client can request a fresh ticket and rejoin.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnDisconnectedFromGameServer, const FString&, Reason);

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

// One equipped item (helmet/vest/weapon1/weapon2 -- see EEquipmentSlot) for
// level-transition carryover only (see CacheStateForLevelTransition):
// UEquipmentComponent's own EquippedSlots storage is entirely separate from
// InventoryGridComponent's grid, so it needs its own snapshot/restore, not
// just FProtoInventoryItemEntry's grid position fields.
USTRUCT(BlueprintType)
struct FProtoEquipmentEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	FName ItemId;

	// EEquipmentSlot, as a plain int32 so this header doesn't need to
	// include EquipmentComponent.h just for the enum.
	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 Slot = 0;
};

// One quick slot assignment, same level-transition-carryover-only reasoning
// as FProtoEquipmentEntry above (UQuickSlotComponent's Slots array is its
// own separate storage too).
USTRUCT(BlueprintType)
struct FProtoQuickSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	int32 StackCount = 1;
};

// One loose item dropped in the open world by an AItemSpawnPoint's roll --
// a fixed world Position instead of a grid slot (see FProtoInventoryItemEntry
// for that). ItemId is the item Data Asset's own object name, same as above.
USTRUCT(BlueprintType)
struct FProtoWorldItemEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	FName ItemId;

	UPROPERTY(BlueprintReadWrite, Category = "ProtoNet")
	FVector Position = FVector::ZeroVector;

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

// Fired on S2C_StashState -- the reply to SendRequestStash, this account's
// full saved contents for StashIndex (empty array if nothing's saved for
// that stash). Unlike OnContainerLootState, this is unicast to the
// requester's account only (see C2S_RequestStash's schema comment), not
// broadcast to other players -- but every AStorageContainer this client has
// placed (e.g. two boxes, StashIndex 0 and 1 -- 문제점09-12.txt #7 "창고
// 두개 1 2") shares this one delegate, so each still needs to filter by
// StashIndex to find its own reply, same "filter by id" reasoning as
// OnContainerLootState's ContainerId.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnStashState, int32, StashIndex, const TArray<FProtoInventoryItemEntry>&, Items);

// Fired whenever the server answers a container loot roll (see
// SendContainerLootRoll) with the authoritative contents for ContainerId --
// either this client's own roll (if it was first) or another client's
// earlier one. AItemContainerBase-derived actors bind this in BeginPlay and
// filter by their own GetContainerId(), so nothing needs a central registry
// of which container is waiting for which reply.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnContainerLootState, int32, ContainerId, const TArray<FProtoInventoryItemEntry>&, Items);

// Same idea as FProtoOnContainerLootState, for an AItemSpawnPoint's
// scattered world drops (see SendItemSpawnRoll) instead of a grid
// container's contents. AItemSpawnPoint binds this in BeginPlay and
// filters by its own spawn point id.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnItemSpawnState, int32, SpawnPointId, const TArray<FProtoWorldItemEntry>&, Items);

// Fired on S2C_ItemDropped -- another player manually threw ItemId out of
// their inventory onto the ground at Position (see C2S_DropItem's schema
// comment). Never fires for this client's own drop (the server excludes
// the sender -- see that message's comment); NetSlotId is already the
// receiver's independently-computed match for whatever id the dropper's
// own optimistic local spawn used (see
// UProtoNetClientSubsystem::ComputeItemDropNetSlotId).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FProtoOnItemDropped, int32, NetSlotId, FName, ItemId, FVector, Position, int32, StackCount);

// Fired on S2C_InteractResult for a DoorOpen/DoorClose interact_type only
// (see SendDoorInteract's schema comment) -- every OTHER client's copy of
// DoorId should match bOpen. Never fires for this client's own toggle (the
// server doesn't echo C2S_InteractRequest back to its sender for doors).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FProtoOnDoorInteract, int32, DoorId, bool, bOpen);

// Fired on every S2C_InteractResult for a Loot interact_type -- both Ok
// (first-claim-wins arbitration, see Session.cpp's C2S_InteractRequest
// case and EchoServer::ClaimItemPickup) AND Denied. Unlike OnDoorInteract,
// Ok DOES fire for this client's own successful pickup (it's broadcast to
// everyone including the requester, since the requester needs to know it
// won before actually adding the item to an inventory) -- bGranted true
// plus PickerPlayerId tells ADropItem::HandlePickupResult whether that's
// itself or another client.
//
// bGranted false (Denied, always unicast to the loser) means exactly the
// same thing visually as "Ok, granted to someone else": this NetSlotId is
// gone, destroy whatever local copy exists, just without an inventory to
// credit. Earlier versions of this delegate never fired for Denied at all
// (the assumption being a loser could just wait for the actual winner's Ok
// broadcast instead) -- that broke down whenever this NetSlotId had
// already been claimed by someone LONG before this particular ADropItem
// instance even existed to hear that original broadcast (e.g. the same
// server process, still up from an earlier test run, already had it
// marked claimed): the item would render as a normal, interactable pickup
// forever, but every attempt would silently do nothing (a "ghost" item).
// Denied being delegated (and destroying the local copy) fixes that
// regardless of whether the claim was truly simultaneous or long-stale.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FProtoOnItemPickupResult, int32, NetSlotId, int32, PickerPlayerId, bool, bGranted);

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

// Fired on S2C_EnemyAttackBroadcast -- purely visual: a server-driven
// enemy_id landed a hit (on SOMEONE, not necessarily this client's own
// player) and every mirrored copy of it should play its attack animation.
// Broadcast to everyone, unlike OnEnemyAttackPlayer's unicast.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FProtoOnEnemyAttackBroadcast, int32, EnemyId);

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
	 로그인/게임 서버 이중 접속 (매칭 서버 설계, step 5)
	-------------------*/
	// Sends C2S_RequestMatch over the LOGIN connection (see Connect/
	// ConnectAndLogin above) -- requires already being logged into a real
	// account (a guest/token-only login has no account to tie a ticket to,
	// see that message's server-side comment, and this returns false
	// without sending anything). Reply arrives via OnMatchTicketReceived or
	// -- if the server has no account for this session -- never arrives at
	// all (there's no explicit failure message for that case).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool RequestMatch();

	// Opens a SEPARATE connection (does not touch the Login connection at
	// all) to the Game server and redeems Ticket in place of a username/
	// password login -- see C2S_JoinMatch's schema comment. HostOverride/
	// PortOverride default to empty/0, meaning "use whatever
	// OnMatchTicketReceived reported" (the normal case); pass explicit
	// values to bypass that (e.g. a hardcoded test server). Success/failure
	// arrive via the SAME OnLoginSucceeded/OnLoginFailed delegates a normal
	// login uses for the S2C_LoginSuccess case (this account's saved
	// progress/inventory restored exactly the same way), plus a dedicated
	// OnJoinMatchFailed for S2C_JoinMatchFail (a failure mode with no
	// equivalent in a plain C2S_Login). From here on, every gameplay Send*
	// helper below (movement, attack, interact, companion, enemy, ...)
	// automatically prefers this Game connection over the Login one -- see
	// SendGameplayPacketBytes.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConnectToGameServerAndJoin(const FString& Ticket, const FString& HostOverride = TEXT(""), int32 PortOverride = 0);

	// Tears down ONLY the Game connection -- the Login connection (and
	// therefore the account session: persistence saves, a future
	// RequestMatch) is untouched. Call this on returning to the hub/
	// SafePlace after a raid (extraction, death, or a manual leave) --
	// mirrors how SetMultiplayerVisualsEnabled(false) already signals
	// "left the shared world" today, just for a connection that can now
	// actually not exist anymore instead of merely going quiet.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void DisconnectFromGameServer();

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool IsConnectedToGameServer() const;

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
	// bOutApplyTransform is now always false from every caller (level-
	// transition carryover AND a fresh DB login alike) -- see
	// CacheStateForLevelTransition's comment and the S2C_LoginSuccess
	// handler's comment for why a server-saved position can't be trusted
	// either (it may belong to a different level's coordinate space, e.g.
	// a raid coordinate saved right before a force-quit, misapplied inside
	// SafePlaceLevel on the next login -- the "강제종료한 플레이어의 위치가
	// 이상한곳으로 고정되는 문제" bug). OutPosition/OutLook are still
	// populated (kept for logging or a future feature) but every level is
	// now always entered at its own PlayerStart regardless.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConsumePendingProgressRestore(FVector& OutPosition, FRotator& OutLook, uint8& OutWeaponType, bool& bOutApplyTransform);

	// OutEquipment/OutQuickSlots are populated either from a real DB login
	// (S2C_LoginSuccess.equipment/quick_slots, see C2S_SaveEquipment/
	// C2S_SaveQuickSlots's schema comments) or from
	// CacheStateForLevelTransition below -- whichever populated OutItems
	// too, since all three are cached and consumed together as one package.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool ConsumePendingInventoryRestore(TArray<FProtoInventoryItemEntry>& OutItems,
		TArray<FProtoEquipmentEntry>& OutEquipment, TArray<FProtoQuickSlotEntry>& OutQuickSlots);

	// Same pending-restore cache as above, but seeded directly from local
	// state instead of a server round-trip -- called by
	// AProtoCharacter::EndPlay(LevelTransition) so weapon/inventory/
	// equipment/quick slots survive moving between levels within the same
	// login session (Test -> Single/Multi via LevelChanger and back). Those
	// transitions don't involve a fresh login, so there's no
	// S2C_LoginSuccess to populate the cache the normal way -- without
	// this, every level change silently dropped whatever the player was
	// holding (including, until Equipment/QuickSlots were added here,
	// anything actually equipped rather than just sitting in the grid).
	// 위치/시선은 이월하지 않는다(목적지 PlayerStart 사용) -- 장착 무기 타입 + 인벤토리/장비/
	// 퀵슬롯만 이월. 위치 복원은 오직 S2C_LoginSuccess 경로에서만.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	void CacheStateForLevelTransition(uint8 WeaponType,
		const TArray<FProtoInventoryItemEntry>& InventoryItems, const TArray<FProtoEquipmentEntry>& Equipment,
		const TArray<FProtoQuickSlotEntry>& QuickSlots);

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
	// no separate type to tell them apart. TargetId must be the item's
	// NetSlotId (see ADropItem::RequestPickup), not GetUniqueID() -- the
	// server arbitrates first-claim-wins on this id and answers via
	// OnItemPickupResult, so it has to mean the same ADropItem instance on
	// every client, which only NetSlotId does.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendInteractLoot(int32 TargetId);

	// Sent by ADoor whenever it toggles (both opening AND closing), so
	// every other client's copy matches. Simple relay, not an
	// ownership/arbitration flow like the loot rolls below -- see
	// S2C_InteractResult's schema comment (Session.cpp's C2S_InteractRequest
	// case) for why. Gated the same way SendInteractLoot effectively is
	// (bMultiplayerVisualsEnabled): a Single-map door has no one else to
	// tell.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendDoorInteract(int32 DoorId, bool bOpen);

	// Non-destructive lookup into every door state this client has EVER
	// been told about (the C2S_Login roster replay AND live OnDoorInteract
	// broadcasts both update the same cache -- see the .cpp handler).
	// ADoor::BeginPlay() calls this immediately, in addition to binding
	// OnDoorInteract for live updates: OnDoorInteract is a plain dynamic
	// multicast delegate, so it doesn't replay past broadcasts to a door
	// that spawns/streams in afterward (same problem
	// ConsumePendingProgressRestore's comment describes for player
	// progress) -- without this, a door already open when a client joins
	// (or when a sublevel streams in) would render shut until someone
	// happened to toggle it again. Unlike the Consume* functions above,
	// this is a repeatable lookup, not a one-shot drain: many doors share
	// this one cache. Returns false (OutIsOpen untouched) if this DoorId
	// has never been toggled -- closed is the default every door already
	// starts as, so there's nothing to catch up on.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool TryGetCachedDoorState(int32 DoorId, bool& OutIsOpen) const;

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
	// Matches ProtoType::Net::MoveFlags::Jump (already in the schema, just
	// never used until now -- see AProtoCharacter::Jump()). One-shot: only
	// ever set on the single out-of-band SendMoveInput a real jump press
	// fires, never on the periodic NetSyncInterval sync, so a remote mirror
	// only ever sees it once per actual jump instead of for the whole time
	// the jumper is airborne.
	static constexpr int32 kMoveFlagJump = 8;

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

	// Same contract/reasoning as SendSaveInventory, for
	// UEquipmentComponent's/UQuickSlotComponent's own separate storage.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSaveEquipment(const TArray<FProtoEquipmentEntry>& Items);

	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSaveQuickSlots(const TArray<FProtoQuickSlotEntry>& Items);

	// SafePlace 개인 창고(dbo.PlayerStash) 요청/저장 -- SendSaveInventory와 동일한 이유로
	// SetMultiplayerVisualsEnabled에 게이팅하지 않는다(계정 영속 데이터, 다른 플레이어가 보는
	// 게 아님). 서버는 OnStashState로 답한다(계정당 유니캐스트, 다른 클라이언트에 브로드캐스트
	// 되지 않음 -- 루팅 컨테이너와 달리 개인 데이터라서). StashIndex는 이 계정의 여러 창고
	// 중 어느 것인지 구분한다(문제점09-12.txt #7 "창고 두개 1 2").
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendRequestStash(int32 StashIndex);

	// Same "always full contents, not a diff" contract as SendSaveInventory.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSaveStash(int32 StashIndex, const TArray<FProtoInventoryItemEntry>& Items);

	// Same formula the dropper uses for its own optimistic local spawn and
	// every other client uses on receiving S2C_ItemDropped -- kept in one
	// place so both sides can never drift apart (see that message's schema
	// comment).
	static int32 ComputeItemDropNetSlotId(uint32 PlayerId, uint32 DropSequence)
	{
		return GetTypeHash(FString::Printf(TEXT("ItemDrop_%u_%u"), PlayerId, DropSequence));
	}

	// Called by UInventoryScreenWidget when a player manually throws an item
	// out of their inventory (drag-out-of-grid / partial-stack drop) -- see
	// C2S_DropItem's schema comment. Gated by bMultiplayerVisualsEnabled: a
	// Single map has no one else to tell, so the caller should fall back to
	// its old plain-local spawn (no NetSlotId) when this returns false,
	// exactly as if this feature didn't exist. On success, OutNetSlotId is
	// the id the caller must assign to the ADropItem it's about to spawn
	// locally -- every other client will independently compute the exact
	// same id from S2C_ItemDropped (see ComputeItemDropNetSlotId), so pickup
	// arbitration (DropItem.h's NetSlotId comment) works correctly if a
	// teammate grabs it first.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendDropItem(FName ItemId, FVector Position, int32 StackCount, int32& OutNetSlotId);

	// See C2S_SetVisible's schema comment -- called by
	// SetMultiplayerVisualsEnabled(false) so other clients despawn this
	// player instead of freezing it in place as a "ghost" (this session
	// stays connected the whole time, it just stops sending move updates).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendSetVisible(bool bVisible);

	// Called once by AProtoCharacter::HandleDeath() (local player only) so
	// every other client ragdolls their mirror of this player too instead
	// of leaving it standing frozen, AND spawns the same death-drop pile
	// Items describes -- see S2C_PlayerDied's schema comment and
	// AProtoCharacter::BuildDeathDropSnapshot/SpawnDeathDropItems.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendPlayerDied(const TArray<FProtoWorldItemEntry>& Items);

	// Reports the local roll an AItemContainerBase-derived actor generated
	// for itself on BeginPlay (see ALootContainer::SeedContents). The
	// server answers via OnContainerLootState with the authoritative
	// contents -- either this roll, if it's the first for ContainerId, or
	// an earlier client's -- so every client agrees on what's in the box.
	// Gated by bMultiplayerVisualsEnabled: containerLoot_ is a server-wide,
	// unpartitioned map (see this function's .cpp comment), so a Single
	// map keeps its roll purely local instead of racing some other
	// session's identically-named container.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendContainerLootRoll(int32 ContainerId, const TArray<FProtoInventoryItemEntry>& Items);

	// Same idea as SendContainerLootRoll (including the
	// bMultiplayerVisualsEnabled gate and why), for an AItemSpawnPoint's
	// scattered world drops (see AItemSpawnPoint::SpawnLoot). The server
	// answers via OnItemSpawnState with the authoritative drops -- either
	// this roll, if it's the first for SpawnPointId, or an earlier
	// client's.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendItemSpawnRoll(int32 SpawnPointId, const TArray<FProtoWorldItemEntry>& Items);

	// Reports the local AI companion's position/facing, throttled (same
	// idea as SendMoveInput), so other clients can mirror it via a
	// placeholder actor (see UpdateRemoteCompanion). Gated by
	// bMultiplayerVisualsEnabled same as every other Send*() helper here --
	// this USED to be a deliberate exception ("what my companion is doing
	// regardless of whether this client wants to see other players"), but
	// that let a companion whose owner left the Multi map keep broadcasting
	// its position from wherever it physically is now, which the server's
	// visibility-blind relay (see EchoServer::SnapshotOtherSessions) forwarded
	// to every still-visible client -- re-spawning the companion puppet
	// S2C_PlayerLeft had just despawned, as a "ghost" that kept reappearing
	// at the wrong spot (and could pull a zombie's UpdateTarget() toward that
	// stale position too). See this function's .cpp comment.
	// Health/bIsDead/WeaponType/bIsAiming/AimPitch all ride along on the
	// same periodic update rather than a separate message -- see
	// C2S_CompanionMoveInput's schema comment. WeaponType is EWeaponType
	// (same uint8 convention as SendWeaponReload/SendWeaponEquip above).
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendCompanionMoveInput(FVector Position, FRotator Look, float Health = 100.0f, bool bIsDead = false,
		uint8 WeaponType = 0, bool bIsAiming = false, float AimPitch = 0.0f);

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
	// OnEnemyClaimResult for the answer. Gated by bMultiplayerVisualsEnabled
	// (see this function's .cpp comment): a Single map's enemy_id claim
	// would otherwise contend with any other session's identically-named,
	// supposedly-unrelated enemy in the same server-wide enemyOwners_ map.
	// Never sending this for a Single map is fine on its own -- an enemy's
	// bIsNetworkOwner already defaults to true, so it just always runs its
	// own local AI, no claim needed when there's no one to claim against.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyClaimRequest(int32 EnemyId);

	// Throttled, called only by whichever client owns EnemyId (i.e. only
	// after OnEnemyClaimResult granted it) -- see C2S_EnemyState's schema
	// comment. Same bMultiplayerVisualsEnabled gate as SendEnemyClaimRequest
	// and for the same reason: with claiming gated off, a Single map's
	// enemies are always their own owner and would otherwise broadcast
	// their state to every other connected session for no reason.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyState(int32 EnemyId, FVector Position, FRotator Look, float Health, bool bIsDead);

	// Called by a NON-owning client whose own local hit detection landed a
	// shot on EnemyId, so the server can relay it to whichever client
	// actually owns that enemy's health. See C2S_EnemyDamage's schema
	// comment. Same bMultiplayerVisualsEnabled gate as SendEnemyClaimRequest
	// -- meaningless once claiming (and therefore ever NOT being the owner)
	// can't happen for a Single map.
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
	// bIsCaller/CallRadius/CallCooldown are AEnemyCaller-only (see
	// AEnemyBase::IsCallerType) -- 0/false for every other enemy type.
	UFUNCTION(BlueprintCallable, Category = "ProtoNet")
	bool SendEnemyRegister(int32 EnemyId, FVector Position, float Health, float MaxHealth, float MoveSpeed, float AttackRange, float AttackDamage, float AttackCooldown, bool bIsCaller, float CallRadius, float CallCooldown);

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
	FProtoOnStashState OnStashState;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnItemDropped OnItemDropped;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnContainerLootState OnContainerLootState;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnItemSpawnState OnItemSpawnState;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnDoorInteract OnDoorInteract;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnItemPickupResult OnItemPickupResult;

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
	FProtoOnEnemyAttackBroadcast OnEnemyAttackBroadcast;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnLoginSucceeded OnLoginSucceeded;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnLoginFailed OnLoginFailed;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnMatchTicketReceived OnMatchTicketReceived;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnJoinMatchFailed OnJoinMatchFailed;

	UPROPERTY(BlueprintAssignable, Category = "ProtoNet")
	FProtoOnDisconnectedFromGameServer OnDisconnectedFromGameServer;

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
	void UpdateRemotePlayer(uint32 PlayerId, const FVector& Location, const FRotator& Rotation, bool bSprinting = false, bool bAiming = false, bool bJumped = false);
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
	// Health/bIsDead/WeaponType/bIsAiming/AimPitch are all mirrored onto
	// the puppet's own Mirrored* properties (its real CombatComponent/
	// AIComponent/Controller are destroyed or never possessed by
	// MarkAsRemotePuppet, so there's nowhere else to derive these from --
	// see ACompanionNPC::Tick's bIsRemotePuppet branch, which feeds them
	// into the same CurrentWeaponType/bIsAiming/AimPitch properties the
	// Animation Blueprint already reads for the real, locally-owned
	// companion). TickRemotePlayers() stops walking it around once
	// bIsMirroredDead. Known gap: reload state and left-hand IK aren't
	// synced, just the equipped weapon and aim direction.
	void UpdateRemoteCompanion(uint32 OwnerId, const FVector& Location, const FRotator& Rotation, float Health, bool bIsDead,
		uint8 WeaponType, bool bIsAiming, float AimPitch);

	// Despawns a remote companion placeholder and clears its tracking
	// entries. Also called from RemoveRemotePlayer -- a player's companion
	// can't outlive them.
	void RemoveRemoteCompanion(uint32 OwnerId);

	/*-------------------
	 패킷 송신 (로그인/게임 연결 중 실제로 나갈 곳 결정)
	-------------------*/
	// Shared low-level framer/sender both connections' public Send*()
	// helpers eventually funnel through -- see SendPacketBytes (Login) and
	// SendGameplayPacketBytes (Game-preferring) below.
	bool SendPacketBytesOnSocket(FSocket* TargetSocket, FCriticalSection& Lock, const TArray<uint8>& PacketBytes);

	// Every gameplay Send*() helper (movement, attack, item use, interact,
	// companion, enemy, ...) calls this instead of SendPacketBytes directly:
	// prefers the Game connection if ConnectToGameServerAndJoin has one up,
	// falling back to the Login connection otherwise -- which reproduces
	// today's single-connection behavior exactly for as long as no one ever
	// calls ConnectToGameServerAndJoin (see this project's Login/Game
	// server split design doc, step 5, for why that fallback matters: the
	// ticket flow is additive, not a replacement for the direct-login path
	// every existing test and, for now, the actual client still uses).
	bool SendGameplayPacketBytes(const TArray<uint8>& PacketBytes);

	// See SetMultiplayerVisualsEnabled(). Gates both the Send*() helpers
	// below and the remote-player-affecting cases in HandleIncomingPacket();
	// login/connection packets are unaffected.
	bool bMultiplayerVisualsEnabled = true;

	// This client's own locally-incrementing counter for SendDropItem -- see
	// C2S_DropItem's schema comment for why it's part of the NetSlotId
	// formula (there's no stable placed-actor name to hash for an ad hoc
	// manual drop, unlike a container/spawn point/enemy).
	uint32 NextDropSequence = 0;

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

	// Login/Game server split (매칭 서버 설계, step 5) -- the SECOND
	// connection, opened by ConnectToGameServerAndJoin and torn down by
	// DisconnectFromGameServer, entirely independent of Socket/Worker/
	// WorkerThread/SendLock/ReceivedPackets/DisconnectReasons above (which
	// keep meaning "the Login connection", unchanged). Null/unconnected
	// whenever the ticket flow hasn't been used -- see
	// SendGameplayPacketBytes's fallback.
	FSocket* GameSocket = nullptr;
	TUniquePtr<FProtoNetReceiveWorker> GameWorker;
	FRunnableThread* GameWorkerThread = nullptr;
	FCriticalSection GameSendLock;
	TQueue<TArray<uint8>, EQueueMode::Mpsc> GameReceivedPackets;
	TQueue<FString, EQueueMode::Mpsc> GameDisconnectReasons;

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
	// true: PendingRestorePosition/Look를 실제로 적용(S2C_LoginSuccess).
	// false: 레벨 이동 이월 -- 위치 무시, PendingRestoreWeaponType만 적용.
	bool bPendingProgressApplyTransform = false;

	bool bHasPendingInventoryRestore = false;
	TArray<FProtoInventoryItemEntry> PendingRestoreInventory;
	// Only ever non-empty via CacheStateForLevelTransition -- see
	// ConsumePendingInventoryRestore's comment.
	TArray<FProtoEquipmentEntry> PendingRestoreEquipment;
	TArray<FProtoQuickSlotEntry> PendingRestoreQuickSlots;

	// See TryGetCachedDoorState above. Every door toggle this client has
	// ever seen (C2S_Login replay or a live broadcast), never cleared on
	// Disconnect -- unlike the pending-restore caches this isn't
	// login-attempt-scoped data, it's just "the last known state of each
	// door", which stays a harmless (if stale) guess even across a
	// reconnect.
	TMap<int32, bool> CachedDoorStates;

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
