// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionCommandRouterComponent.h"
#include "CompanionAIComponent.h"
#include "CompanionBrainComponent.h"
#include "CompanionSpeechComponent.h"
#include "CompanionGameplayTags.h"
#include "CompanionLog.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/Item/DropItem.h"
#include "GameplayTagAssetInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "TimerManager.h"
#include "Engine/World.h"

UCompanionCommandRouterComponent::UCompanionCommandRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	KeywordRules.Add({ { TEXT("따라와"), TEXT("따라와요"), TEXT("가자") }, ECompanionCommandType::Follow });
	KeywordRules.Add({ { TEXT("멈춰"), TEXT("대기"), TEXT("기다려") }, ECompanionCommandType::Stop });
	KeywordRules.Add({ { TEXT("싸워"), TEXT("공격"), TEXT("싸우자") }, ECompanionCommandType::Engage });
	KeywordRules.Add({ { TEXT("탐색"), TEXT("둘러봐"), TEXT("살펴봐"), TEXT("찾아봐") }, ECompanionCommandType::Explore });
	KeywordRules.Add({ { TEXT("가") }, ECompanionCommandType::MoveHere });

	VisionQueryKeywords = { TEXT("보여"), TEXT("뭐야"), TEXT("저건"), TEXT("저게") };
}

void UCompanionCommandRouterComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		AIComponent = Owner->FindComponentByClass<UCompanionAIComponent>();
		BrainComponent = Owner->FindComponentByClass<UCompanionBrainComponent>();
		SpeechComponent = Owner->FindComponentByClass<UCompanionSpeechComponent>();
		VisionCapture = Owner->FindComponentByClass<USceneCaptureComponent2D>();
	}

	if (AIComponent.IsValid())
	{
		AIComponent->OnMoveCommandBlocked.AddDynamic(this, &UCompanionCommandRouterComponent::HandleMoveCommandBlocked);
	}

	if (VisionCapture.IsValid())
	{
		VisionRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		VisionRenderTarget->InitAutoFormat(VisionCaptureResolution, VisionCaptureResolution);
		VisionRenderTarget->UpdateResourceImmediate(true);
		VisionCapture->TextureTarget = VisionRenderTarget;
	}

	LastPlayerSpokeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	ScheduleNextIdleChatter();
}

ECompanionCommandType UCompanionCommandRouterComponent::MatchKeywordCommand(const FString& Text) const
{
	// "가"처럼 1글자 키워드는 부분 문자열로 검사하면 "생각", "가끔"처럼 무관한 단어에도
	// 걸려 오작동한다 - 공백 기준 토큰과 완전히 일치할 때만 인정한다. 2글자 이상 키워드는
	// 조사가 붙는 한국어 특성상 기존처럼 부분 일치를 유지한다.
	TArray<FString> Tokens;
	Text.ParseIntoArrayWS(Tokens);

	for (const FCompanionKeywordRule& Rule : KeywordRules)
	{
		for (const FString& Keyword : Rule.Keywords)
		{
			if (Keyword.IsEmpty())
			{
				continue;
			}

			if (Keyword.Len() <= 1)
			{
				if (Tokens.Contains(Keyword))
				{
					return Rule.Command;
				}
			}
			else if (Text.Contains(Keyword))
			{
				return Rule.Command;
			}
		}
	}

	return ECompanionCommandType::None;
}

bool UCompanionCommandRouterComponent::MatchesVisionQuery(const FString& Text) const
{
	for (const FString& Keyword : VisionQueryKeywords)
	{
		if (!Keyword.IsEmpty() && Text.Contains(Keyword))
		{
			return true;
		}
	}

	return false;
}

void UCompanionCommandRouterComponent::HandleTranscribed(const FString& RecognizedText)
{
	if (GetWorld())
	{
		LastPlayerSpokeTime = GetWorld()->GetTimeSeconds();
	}

	const ECompanionCommandType Command = MatchKeywordCommand(RecognizedText);
	if (Command != ECompanionCommandType::None)
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Router] 키워드 명령 인식: \"%s\" -> %s"),
			*RecognizedText, *UEnum::GetValueAsString(Command));
		ExecuteCommand(Command);
		return;
	}

	if (MatchesVisionQuery(RecognizedText))
	{
		if (AIComponent.IsValid() && AIComponent->IsCombatEngaged())
		{
			UE_LOG(LogCompanionAI, Log, TEXT("[Router] 비전 질의 감지했지만 전투 중이라 무시: \"%s\""), *RecognizedText);
			if (SpeechComponent.IsValid())
			{
				SpeechComponent->Speak(BusyDuringCombatReply);
			}
			return;
		}

		UE_LOG(LogCompanionAI, Log, TEXT("[Router] 비전 질의 감지: \"%s\" -> 화면 캡처 후 Brain 요청"), *RecognizedText);
		TriggerVisionReply(RecognizedText);
		return;
	}

	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 키워드 미매치, Brain 텍스트 폴백: \"%s\""), *RecognizedText);
	if (BrainComponent.IsValid())
	{
		BrainComponent->RequestReply(RecognizedText);
	}
}

void UCompanionCommandRouterComponent::HandleActionRequested(const FString& ActionName, const FString& ActionArg)
{
	UE_LOG(LogCompanionAI, Log, TEXT("[Router] Brain 액션 수신: %s"), *ActionName);

	if (ActionName == TEXT("follow"))
	{
		ExecuteCommand(ECompanionCommandType::Follow);
	}
	else if (ActionName == TEXT("stop"))
	{
		ExecuteCommand(ECompanionCommandType::Stop);
	}
	else if (ActionName == TEXT("engage"))
	{
		ExecuteCommand(ECompanionCommandType::Engage);
	}
	else if (ActionName == TEXT("move_to"))
	{
		ExecuteCommand(ECompanionCommandType::MoveHere);
	}
	else if (ActionName == TEXT("explore"))
	{
		ExecuteCommand(ECompanionCommandType::Explore);
	}
	else if (ActionName == TEXT("give_item"))
	{
		TryGiveItem(ActionArg);
	}
	else
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Router] 알 수 없는 액션: %s"), *ActionName);
	}
}

void UCompanionCommandRouterComponent::HandleMoveCommandBlocked()
{
	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 이동 명령이 정체로 포기됨 -> 확인 대사"));
	if (SpeechComponent.IsValid() && !MoveBlockedLine.IsEmpty())
	{
		SpeechComponent->Speak(MoveBlockedLine);
	}
}

void UCompanionCommandRouterComponent::TryGiveItem(const FString& ItemNameQuery)
{
	AActor* Owner = GetOwner();
	UInventoryGridComponent* Inventory = Owner ? Owner->FindComponentByClass<UInventoryGridComponent>() : nullptr;

	if (Owner && Inventory && !ItemNameQuery.IsEmpty())
	{
		for (const FInventoryItemInstance& Item : Inventory->Items)
		{
			if (!Item.ItemData)
			{
				continue;
			}

			const FString DisplayName = Item.ItemData->DisplayName.ToString();
			// Gemini가 목록 이름을 정확히 그대로 주는 걸 우선 기대하지만, 약간의 표현 차이(조사 등)도
			// 허용하기 위해 양방향 부분 일치까지 인정한다.
			const bool bMatches = DisplayName.Contains(ItemNameQuery, ESearchCase::IgnoreCase)
				|| ItemNameQuery.Contains(DisplayName, ESearchCase::IgnoreCase);
			if (!bMatches)
			{
				continue;
			}

			UWorld* World = GetWorld();
			if (!World)
			{
				return;
			}

			const int32 StackCount = FMath::Max(1, Item.StackCount);
			UItemDataBase* ItemData = Item.ItemData;
			const FGuid InstanceId = Item.InstanceId;

			// FTransform(Rotation, Location) 생성자는 스케일을 (1,1,1)로 고정하고, SpawnActorDeferred +
			// FinishSpawning은 이 트랜스폼을 루트 컴포넌트에 그대로 적용해버려서, 블루프린트에서
			// StaticMeshComp에 설정해둔 스케일 값이 스폰 순간 항상 1로 덮어써진다 - 클래스 기본값(CDO)의
			// 스케일을 읽어와 그대로 유지시킨다.
			FVector DefaultScale = FVector::OneVector;
			if (const ADropItem* DefaultDropItem = GetDefault<ADropItem>(ADropItem::StaticClass()))
			{
				if (DefaultDropItem->StaticMeshComp)
				{
					DefaultScale = DefaultDropItem->StaticMeshComp->GetRelativeScale3D();
				}
			}

			const FVector SpawnLocation = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 150.0f + FVector(0.0f, 0.0f, 50.0f);
			const FTransform SpawnTransform(Owner->GetActorRotation(), SpawnLocation, DefaultScale);

			// ItemData는 ADropItem::OnConstruction이 메시를 붙이는 데 쓰이므로, 스폰 후 대입하면
			// 늦다 - Deferred 스폰으로 먼저 세팅한 뒤 FinishSpawning에서 OnConstruction이 돌게 한다.
			ADropItem* Drop = World->SpawnActorDeferred<ADropItem>(ADropItem::StaticClass(), SpawnTransform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (Drop)
			{
				Drop->ItemData = ItemData;
				Drop->StackCount = StackCount;
				Drop->FinishSpawning(SpawnTransform);

				Inventory->RemoveInstanceById(InstanceId);

				if (SpeechComponent.IsValid())
				{
					SpeechComponent->Speak(FString::Printf(TEXT("%s 여기 놔둘게!"), *DisplayName));
				}
			}
			return;
		}
	}

	UE_LOG(LogCompanionAI, Log, TEXT("[Router] give_item 대상 못 찾음: \"%s\""), *ItemNameQuery);
	if (SpeechComponent.IsValid())
	{
		SpeechComponent->Speak(GiveItemNotFoundLine);
	}
}

void UCompanionCommandRouterComponent::ExecuteCommand(ECompanionCommandType Command)
{
	if (!AIComponent.IsValid())
	{
		return;
	}

	FString AckLine;

	switch (Command)
	{
	case ECompanionCommandType::Follow:
		AIComponent->CommandFollow();
		AckLine = FollowAckLine;
		break;
	case ECompanionCommandType::Stop:
		AIComponent->CommandStop();
		AckLine = StopAckLine;
		break;
	case ECompanionCommandType::Engage:
		AIComponent->CommandEngage();
		AckLine = EngageAckLine;
		break;
	case ECompanionCommandType::MoveHere:
		PerformMoveToWherePlayerIsLooking();
		AckLine = MoveHereAckLine;
		break;
	case ECompanionCommandType::Explore:
		AIComponent->CommandExplore();
		AckLine = ExploreAckLine;
		break;
	default:
		break;
	}

	// 키워드/Function Calling 경로는 Brain(LLM)을 거치지 않아 대사가 없었으므로, 명령을
	// 수행했다는 걸 플레이어가 알 수 있게 짧은 확인 대사를 즉시 들려준다(지연 없음).
	if (!AckLine.IsEmpty())
	{
		UE_LOG(LogCompanionAI, Log, TEXT("[Router] 명령 처리 완료: %s -> \"%s\""),
			*UEnum::GetValueAsString(Command), *AckLine);

		if (SpeechComponent.IsValid())
		{
			SpeechComponent->Speak(AckLine);
		}
	}
}

void UCompanionCommandRouterComponent::PerformMoveToWherePlayerIsLooking()
{
	if (!AIComponent.IsValid())
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager || !GetWorld())
	{
		return;
	}

	const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = PC->PlayerCameraManager->GetCameraRotation().Vector();
	const FVector TraceEnd = CameraLocation + CameraForward * MoveCommandTraceRange;

	FCollisionQueryParams Params(TEXT("CompanionMoveCommand"), false, GetOwner());
	if (APawn* PlayerPawn = PC->GetPawn())
	{
		Params.AddIgnoredActor(PlayerPawn);
	}
	if (AActor* Owner = GetOwner())
	{
		Params.AddIgnoredActor(Owner);
	}

	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, TraceEnd, ECC_Visibility, Params);

	if (bHit && Hit.GetActor())
	{
		if (const IGameplayTagAssetInterface* TagInterface = Cast<IGameplayTagAssetInterface>(Hit.GetActor()))
		{
			FGameplayTagContainer OwnedTags;
			TagInterface->GetOwnedGameplayTags(OwnedTags);
			if (OwnedTags.HasTag(TAG_Companion_Interactable) || OwnedTags.HasTag(TAG_Companion_Waypoint))
			{
				UE_LOG(LogCompanionAI, Log, TEXT("[Router] 태그된 액터로 이동: %s"), *Hit.GetActor()->GetName());
				AIComponent->CommandMoveToActor(Hit.GetActor());
				return;
			}
		}
	}

	const FVector Destination = bHit ? Hit.ImpactPoint : TraceEnd;
	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 지점으로 이동: %s (레이캐스트 히트=%s)"),
		*Destination.ToString(), bHit ? TEXT("true") : TEXT("false"));
	AIComponent->CommandMoveToLocation(Destination);
}

void UCompanionCommandRouterComponent::TriggerVisionReply(const FString& UserText)
{
	if (!BrainComponent.IsValid())
	{
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!VisionCapture.IsValid() || !VisionRenderTarget || !PC || !PC->PlayerCameraManager)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Router] 비전 캡처 불가 -> 이미지 없이 Brain 요청"));
		BrainComponent->RequestReply(UserText);
		return;
	}

	// 플레이어 시점 카메라에 그대로 정렬하면 3인칭/오버숄더 상태에서 플레이어 자신의 몸이 화면에
	// 잡혀 Gemini가 "저게 뭐야?" 질문에 플레이어 자신을 좀비 등으로 오인할 수 있다 - 캡처에서 제외.
	if (APawn* PlayerPawn = PC->GetPawn())
	{
		VisionCapture->HiddenActors.AddUnique(PlayerPawn);
	}

	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 플레이어 시점으로 화면 캡처 수행"));
	VisionCapture->SetWorldLocation(PC->PlayerCameraManager->GetCameraLocation());
	VisionCapture->SetWorldRotation(PC->PlayerCameraManager->GetCameraRotation());
	VisionCapture->FOVAngle = PC->PlayerCameraManager->GetFOVAngle();
	VisionCapture->CaptureScene();

	FRenderTarget* RenderTargetResource = VisionRenderTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> RawPixels;
	if (!RenderTargetResource || !RenderTargetResource->ReadPixels(RawPixels))
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Router] 렌더타겟 픽셀 읽기 실패 -> 이미지 없이 Brain 요청"));
		BrainComponent->RequestReply(UserText);
		return;
	}

	for (FColor& Pixel : RawPixels)
	{
		Pixel.A = 255;
	}

	TArray64<uint8> CompressedBytes64;
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (ImageWrapper.IsValid() && ImageWrapper->SetRaw(RawPixels.GetData(), RawPixels.Num() * sizeof(FColor),
		VisionRenderTarget->SizeX, VisionRenderTarget->SizeY, ERGBFormat::BGRA, 8))
	{
		CompressedBytes64 = ImageWrapper->GetCompressed();
	}

	if (CompressedBytes64.Num() == 0)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Router] PNG 압축 실패 -> 이미지 없이 Brain 요청"));
		BrainComponent->RequestReply(UserText);
		return;
	}

	TArray<uint8> CompressedBytes;
	CompressedBytes.Append(CompressedBytes64.GetData(), static_cast<int32>(CompressedBytes64.Num()));

	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 이미지(%d bytes) 포함 Brain 요청: \"%s\""), CompressedBytes.Num(), *UserText);
	BrainComponent->RequestReplyWithImage(UserText, CompressedBytes);
}

void UCompanionCommandRouterComponent::ScheduleNextIdleChatter()
{
	if (!bIdleChatterEnabled || !GetWorld())
	{
		return;
	}

	const float Delay = FMath::FRandRange(FMath::Max(1.0f, IdleChatterMinInterval), FMath::Max(IdleChatterMinInterval, IdleChatterMaxInterval));
	GetWorld()->GetTimerManager().SetTimer(IdleChatterTimerHandle, this, &UCompanionCommandRouterComponent::TryIdleChatter, Delay, false);
}

void UCompanionCommandRouterComponent::TryIdleChatter()
{
	// 발화 여부와 무관하게 다음 사이클을 계속 예약한다(끄고 켜는 토글은 bIdleChatterEnabled로).
	ScheduleNextIdleChatter();

	if (!bIdleChatterEnabled || !BrainComponent.IsValid() || !GetWorld())
	{
		return;
	}

	if (AIComponent.IsValid() && AIComponent->IsCombatEngaged())
	{
		return;
	}

	const float TimeSinceLastSpeech = GetWorld()->GetTimeSeconds() - LastPlayerSpokeTime;
	if (TimeSinceLastSpeech < IdleChatterMinInterval)
	{
		return;
	}

	UE_LOG(LogCompanionAI, Log, TEXT("[Router] 침묵 %.0f초 경과 -> 혼잣말 유도"), TimeSinceLastSpeech);
	BrainComponent->RequestAmbientLine();
}
