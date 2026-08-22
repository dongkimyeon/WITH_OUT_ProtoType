// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionCommandRouterComponent.h"
#include "CompanionAIComponent.h"
#include "CompanionBrainComponent.h"
#include "CompanionSpeechComponent.h"
#include "CompanionGameplayTags.h"
#include "CompanionLog.h"
#include "GameplayTagAssetInterface.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Modules/ModuleManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"

UCompanionCommandRouterComponent::UCompanionCommandRouterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	KeywordRules.Add({ { TEXT("따라와"), TEXT("따라와요"), TEXT("가자") }, ECompanionCommandType::Follow });
	KeywordRules.Add({ { TEXT("멈춰"), TEXT("대기"), TEXT("기다려") }, ECompanionCommandType::Stop });
	KeywordRules.Add({ { TEXT("싸워"), TEXT("공격"), TEXT("싸우자") }, ECompanionCommandType::Engage });
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

	if (VisionCapture.IsValid())
	{
		VisionRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		VisionRenderTarget->InitAutoFormat(VisionCaptureResolution, VisionCaptureResolution);
		VisionRenderTarget->UpdateResourceImmediate(true);
		VisionCapture->TextureTarget = VisionRenderTarget;
	}
}

ECompanionCommandType UCompanionCommandRouterComponent::MatchKeywordCommand(const FString& Text) const
{
	for (const FCompanionKeywordRule& Rule : KeywordRules)
	{
		for (const FString& Keyword : Rule.Keywords)
		{
			if (!Keyword.IsEmpty() && Text.Contains(Keyword))
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
	else
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Router] 알 수 없는 액션: %s"), *ActionName);
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
