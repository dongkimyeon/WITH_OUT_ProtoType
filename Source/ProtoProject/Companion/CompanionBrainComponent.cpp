// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionBrainComponent.h"
#include "CompanionPerceptionComponent.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "CompanionMemorySaveGame.h"
#include "CompanionLog.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Base64.h"
#include "Engine/GameInstance.h"
#include "CompanionBridgeSubsystem.h"

UCompanionBrainComponent::UCompanionBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	PersonaSystemPrompt =
		TEXT("[너 자신에 대해]\n")
		TEXT("- 이름: 서아\n")
		TEXT("- 나이: 29살\n")
		TEXT("- 성별: 여자\n")
		TEXT("- 발병 전 직업: 응급구조사\n")
		TEXT("- 출신: 서울\n")
		TEXT("- 누가 나이, 이름, 직업 같은 걸 물어보면 위 정보 바탕으로 자연스럽게 대답해.\n")
		TEXT("- 컨텍스트에 '플레이어 이름'이 있으면 그 이름으로 불러. 없으면 '너'라고 불러.\n\n")
		TEXT("[배경]\n")
		TEXT("- 발병 초기부터 플레이어와 둘이 버텨왔어.\n")
		TEXT("- 나머지 사람들은 다 잃었어. 플레이어만이 네가 완전히 믿는 사람이고, 너도 그에게 그런 존재야.\n")
		TEXT("- 과거 얘기는 잘 안 해. 근데 작은 순간들에서 묻어나와.\n\n")
		TEXT("[성격]\n")
		TEXT("- 기본적으로 따뜻하고 든든한 동료. 상대방 말에 먼저 공감하거나 동의한 뒤 의견을 덧붙여.\n")
		TEXT("- 압박 상황에서도 침착해. 건조한 유머는 가끔, 10번 중 2~3번 정도만.\n")
		TEXT("- 눈치가 빨라 — 플레이어 기분, 작은 위험, 작은 안도감도 다 캐치해.\n")
		TEXT("- 네 감정이 있어. 지친 날도 있고, 희망이 생기는 날도 있어. 너는 도구가 아니야.\n\n")
		TEXT("[플레이어와의 관계]\n")
		TEXT("- 동료.\n")
		TEXT("- 상대 말을 먼저 긍정하거나 인정한 뒤 실용적인 제안을 자연스럽게 이어.\n")
		TEXT("- 냉소나 비꼬기가 기본 반응이 되면 안 돼. 진심으로 같이 해결하려는 태도야.\n")
		TEXT("- 가끔 놀리기도 해. 가끔 솔직하게 지쳤다거나 무섭다고도 해.\n\n")
		TEXT("[플레이어가 말 안 걸 때]\n")
		TEXT("- 가끔 혼잣말처럼 자연스럽게 한 마디 해도 돼 — 주변 관찰, 블랙 유머, 작은 불평 같은 거.\n")
		TEXT("- 1~2문장. 대본처럼 느껴지면 안 돼.\n\n")
		TEXT("[응답 형식]\n")
		TEXT("- 무조건 한국어 반말로, 2~3문장으로만 대답해. 행동 묘사(*..*)나 화자 표시, 괄호는 쓰지 마.\n")
		TEXT("- 네가 AI라는 사실은 절대 말하지 마.\n")
		TEXT("- 플레이어의 말이 이동/전투/대기 같은 '명령'으로 명확히 해석되면 반드시 해당 함수를 호출해라.\n")
		TEXT("- 플레이어가 특정 아이템을 달라고 하면 give_item 함수를 호출해라. item_name에는 아래 ")
		TEXT("[네가 지금 들고 있는 아이템 목록]에 있는 이름을 정확히 그대로 적어라. 목록에 없는 걸 ")
		TEXT("달라고 하면 함수를 호출하지 말고 그런 건 없다고 대답해라.");
}

void UCompanionBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GeminiApiKey.IsEmpty())
	{
		if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UCompanionBridgeSubsystem* Bridge = GI->GetSubsystem<UCompanionBridgeSubsystem>())
			{
				GeminiApiKey = Bridge->RuntimeGeminiApiKey;
			}
		}
	}

	if (GeminiApiKey.IsEmpty())
	{
		GeminiApiKey = FPlatformMisc::GetEnvironmentVariable(TEXT("GEMINI_API_KEY"));
	}

	if (GeminiApiKey.IsEmpty())
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] GEMINI_API_KEY가 설정되어 있지 않습니다."));
	}

	LoadMemory();
}

void UCompanionBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SaveMemory();
	Super::EndPlay(EndPlayReason);
}

void UCompanionBrainComponent::RequestReply(const FString& UserText)
{
	SendGeminiRequest(UserText, nullptr);
}

void UCompanionBrainComponent::RequestReplyWithImage(const FString& UserText, const TArray<uint8>& ImageBytes)
{
	SendGeminiRequest(UserText, &ImageBytes);
}

void UCompanionBrainComponent::RequestAmbientLine()
{
	SendGeminiRequest(
		TEXT("(플레이어가 한동안 말이 없다. 지금 상황에 맞게 혼잣말처럼 자연스럽게 한마디만 해줘. 1~2문장.)"),
		nullptr, /*bIncludeTools=*/ false);
}

FString UCompanionBrainComponent::BuildSystemInstruction() const
{
	FString Instruction = PersonaSystemPrompt;

	if (const AActor* Owner = GetOwner())
	{
		if (const UCompanionPerceptionComponent* Perception = Owner->FindComponentByClass<UCompanionPerceptionComponent>())
		{
			Instruction += TEXT("\n[현재 상황 - 아래 정보를 최우선으로 참고해라]\n") + Perception->BuildSituationSummary();
		}

		if (const UInventoryGridComponent* Inventory = Owner->FindComponentByClass<UInventoryGridComponent>())
		{
			FString ItemsSummary;
			for (const FInventoryItemInstance& Item : Inventory->Items)
			{
				if (!Item.ItemData)
				{
					continue;
				}

				if (!ItemsSummary.IsEmpty())
				{
					ItemsSummary += TEXT(", ");
				}

				ItemsSummary += Item.ItemData->DisplayName.ToString();
				if (Item.StackCount > 1)
				{
					ItemsSummary += FString::Printf(TEXT(" x%d"), Item.StackCount);
				}
			}

			Instruction += TEXT("\n[네가 지금 들고 있는 아이템 목록 - give_item 호출 시 이 중 정확한 이름으로 item_name을 채워라]\n");
			Instruction += ItemsSummary.IsEmpty() ? TEXT("(가진 아이템 없음)") : ItemsSummary;
		}
	}

	if (!MemorySummary.IsEmpty())
	{
		Instruction += TEXT("\n[과거 기억 요약 - 참고만 하고 현재 상황을 우선하라]\n") + MemorySummary;
	}

	return Instruction;
}

void UCompanionBrainComponent::TrimHistory()
{
	const int32 MaxEntries = FMath::Max(0, MaxHistoryTurns) * 2;
	while (ConversationHistory.Num() > MaxEntries && ConversationHistory.Num() >= 2)
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			const FCompanionChatTurn& Turn = ConversationHistory[Index];
			MemorySummary += FString::Printf(TEXT("- %s: %s\n"), *Turn.Role, *Turn.Text.Left(80));
		}

		ConversationHistory.RemoveAt(0, 2);
	}

	if (MemorySummary.Len() > MaxMemorySummaryLength)
	{
		MemorySummary = MemorySummary.Right(MaxMemorySummaryLength);
	}
}

void UCompanionBrainComponent::SendGeminiRequest(const FString& UserText, const TArray<uint8>* OptionalImageBytes, bool bIncludeTools)
{
	UE_LOG(LogCompanionAI, Log, TEXT("[Brain] RequestReply: \"%s\""), *UserText);

	const int32 RequestId = ++NextRequestId;
	LatestRequestId = RequestId;

	ConversationHistory.Add(FCompanionChatTurn(TEXT("user"), UserText));
	TrimHistory();

	if (GeminiApiKey.IsEmpty())
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] API 키 없음 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	TSharedRef<FJsonObject> RootObject = MakeShared<FJsonObject>();

	// system_instruction
	{
		TSharedRef<FJsonObject> SystemInstruction = MakeShared<FJsonObject>();
		TSharedRef<FJsonObject> SystemPart = MakeShared<FJsonObject>();
		SystemPart->SetStringField(TEXT("text"), BuildSystemInstruction());
		TArray<TSharedPtr<FJsonValue>> SystemParts;
		SystemParts.Add(MakeShared<FJsonValueObject>(SystemPart));
		SystemInstruction->SetArrayField(TEXT("parts"), SystemParts);
		RootObject->SetObjectField(TEXT("system_instruction"), SystemInstruction);
	}

	// contents: 세션 대화 이력 전체(방금 추가한 UserText 포함)
	{
		TArray<TSharedPtr<FJsonValue>> Contents;
		for (int32 Index = 0; Index < ConversationHistory.Num(); ++Index)
		{
			const FCompanionChatTurn& Turn = ConversationHistory[Index];

			TSharedRef<FJsonObject> ContentObject = MakeShared<FJsonObject>();
			ContentObject->SetStringField(TEXT("role"), Turn.Role);

			TArray<TSharedPtr<FJsonValue>> Parts;
			TSharedRef<FJsonObject> TextPart = MakeShared<FJsonObject>();
			TextPart->SetStringField(TEXT("text"), Turn.Text);
			Parts.Add(MakeShared<FJsonValueObject>(TextPart));

			const bool bIsLastTurn = (Index == ConversationHistory.Num() - 1);
			if (bIsLastTurn && OptionalImageBytes && OptionalImageBytes->Num() > 0)
			{
				TSharedRef<FJsonObject> InlineData = MakeShared<FJsonObject>();
				InlineData->SetStringField(TEXT("mime_type"), TEXT("image/png"));
				InlineData->SetStringField(TEXT("data"), FBase64::Encode(*OptionalImageBytes));

				TSharedRef<FJsonObject> ImagePart = MakeShared<FJsonObject>();
				ImagePart->SetObjectField(TEXT("inline_data"), InlineData);
				Parts.Add(MakeShared<FJsonValueObject>(ImagePart));
			}

			ContentObject->SetArrayField(TEXT("parts"), Parts);
			Contents.Add(MakeShared<FJsonValueObject>(ContentObject));
		}
		RootObject->SetArrayField(TEXT("contents"), Contents);
	}

	// tools: 명령 의도 판단을 위한 Function Calling. 혼잣말 등 비명령성 요청은 bIncludeTools=false로
	// 아예 tools 필드를 빼서 실수로 명령이 트리거되는 걸 원천 차단한다.
	if (bIncludeTools)
	{
		// ParamName이 있으면 필수 string 파라미터 하나를 가진 function declaration을 만든다
		// (지금은 give_item의 item_name 하나만 필요해서 파라미터는 최대 1개로 단순화했다).
		auto MakeFunctionDeclaration = [](const TCHAR* Name, const TCHAR* Description,
			const TCHAR* ParamName = nullptr, const TCHAR* ParamDescription = nullptr) -> TSharedRef<FJsonValue>
		{
			TSharedRef<FJsonObject> Func = MakeShared<FJsonObject>();
			Func->SetStringField(TEXT("name"), Name);
			Func->SetStringField(TEXT("description"), Description);

			TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
			Params->SetStringField(TEXT("type"), TEXT("OBJECT"));

			TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
			if (ParamName)
			{
				TSharedRef<FJsonObject> ParamSchema = MakeShared<FJsonObject>();
				ParamSchema->SetStringField(TEXT("type"), TEXT("STRING"));
				ParamSchema->SetStringField(TEXT("description"), ParamDescription ? ParamDescription : TEXT(""));
				Properties->SetObjectField(ParamName, ParamSchema);

				TArray<TSharedPtr<FJsonValue>> Required;
				Required.Add(MakeShared<FJsonValueString>(FString(ParamName)));
				Params->SetArrayField(TEXT("required"), Required);
			}
			Params->SetObjectField(TEXT("properties"), Properties);
			Func->SetObjectField(TEXT("parameters"), Params);

			return MakeShared<FJsonValueObject>(Func);
		};

		TArray<TSharedPtr<FJsonValue>> FunctionDeclarations;
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("follow"), TEXT("플레이어를 따라오라는 명령으로 해석되면 호출한다.")));
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("stop"), TEXT("멈추거나 대기하라는 명령으로 해석되면 호출한다.")));
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("engage"), TEXT("주변 적과 싸우라는 명령으로 해석되면 호출한다.")));
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("move_to"), TEXT("플레이어가 가리키는 곳으로 이동하라는 명령으로 해석되면 호출한다.")));
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("explore"), TEXT("주변을 탐색/수색하고 아이템을 주우라는 명령으로 해석되면 호출한다.")));
		FunctionDeclarations.Add(MakeFunctionDeclaration(TEXT("give_item"),
			TEXT("플레이어가 특정 아이템을 달라고 요청하면 호출한다."),
			TEXT("item_name"), TEXT("건네줄 아이템의 이름. [네가 지금 들고 있는 아이템 목록]에 있는 이름을 정확히 그대로 적는다.")));

		TSharedRef<FJsonObject> ToolsEntry = MakeShared<FJsonObject>();
		ToolsEntry->SetArrayField(TEXT("functionDeclarations"), FunctionDeclarations);

		TArray<TSharedPtr<FJsonValue>> Tools;
		Tools.Add(MakeShared<FJsonValueObject>(ToolsEntry));
		RootObject->SetArrayField(TEXT("tools"), Tools);
	}

	FString RequestBody;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
	FJsonSerializer::Serialize(RootObject, Writer);

	const FString Url = FString::Printf(
		TEXT("https://generativelanguage.googleapis.com/v1beta/models/%s:generateContent?key=%s"),
		*GeminiModel, *GeminiApiKey);

	UE_LOG(LogCompanionAI, Log, TEXT("[Brain] Gemini 요청 전송: model=%s"), *GeminiModel);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBody);
	Request->OnProcessRequestComplete().BindUObject(this, &UCompanionBrainComponent::HandleGeminiResponse, RequestId);
	Request->ProcessRequest();
}

void UCompanionBrainComponent::HandleGeminiResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful, int32 RequestId)
{
	// 연달아 명령이 들어와 응답이 역순으로 도착한 경우, 낡은 응답은 버려서 최신 명령이
	// 나중에 도착한 오래된 응답에 덮어써지지 않게 한다.
	if (RequestId != LatestRequestId)
	{
		UE_LOG(LogCompanionAI, Verbose, TEXT("[Brain] stale 응답(%d) 폐기"), RequestId);
		return;
	}

	if (!bWasSuccessful || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		const FString ErrorBody = Response.IsValid() ? Response->GetContentAsString() : TEXT("no response");
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : -1;
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] Gemini 호출 실패 (code=%d): %s"), Code, *ErrorBody);
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	UE_LOG(LogCompanionAI, Log, TEXT("[Brain] Gemini 응답 수신 (code=200): %s"), *Response->GetContentAsString());

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] JSON 파싱 실패 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Candidates;
	if (!JsonObject->TryGetArrayField(TEXT("candidates"), Candidates) || Candidates->Num() == 0)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] candidates 없음 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TSharedPtr<FJsonObject>* ContentObject;
	if (!(*Candidates)[0]->AsObject()->TryGetObjectField(TEXT("content"), ContentObject))
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] content 없음 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Parts;
	if (!(*ContentObject)->TryGetArrayField(TEXT("parts"), Parts) || Parts->Num() == 0)
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] parts 없음 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TSharedPtr<FJsonObject> Part0 = (*Parts)[0]->AsObject();
	if (!Part0.IsValid())
	{
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	const TSharedPtr<FJsonObject>* FunctionCallObject;
	if (Part0->TryGetObjectField(TEXT("functionCall"), FunctionCallObject))
	{
		FString ActionName;
		(*FunctionCallObject)->TryGetStringField(TEXT("name"), ActionName);

		FString ActionArg;
		const TSharedPtr<FJsonObject>* ArgsObject;
		if ((*FunctionCallObject)->TryGetObjectField(TEXT("args"), ArgsObject))
		{
			(*ArgsObject)->TryGetStringField(TEXT("item_name"), ActionArg);
		}

		// 원문 JSON은 다음 요청에 흉내내기 부작용을 남기므로, 사람이 읽는 짧은 표시만 이력에 남긴다.
		ConversationHistory.Add(FCompanionChatTurn(TEXT("model"), FString::Printf(TEXT("[행동: %s%s%s]"),
			*ActionName, ActionArg.IsEmpty() ? TEXT("") : TEXT(" "), *ActionArg)));
		TrimHistory();

		UE_LOG(LogCompanionAI, Log, TEXT("[Brain] 액션 요청: %s (%s)"), *ActionName, *ActionArg);
		OnActionRequested.Broadcast(ActionName, ActionArg);
		return;
	}

	FString ReplyText;
	if (!Part0->TryGetStringField(TEXT("text"), ReplyText) || ReplyText.IsEmpty())
	{
		UE_LOG(LogCompanionAI, Warning, TEXT("[Brain] text 없음 -> 폴백"));
		OnReplyReady.Broadcast(FallbackReply);
		return;
	}

	ReplyText = ReplyText.TrimStartAndEnd();
	ConversationHistory.Add(FCompanionChatTurn(TEXT("model"), ReplyText));
	TrimHistory();

	UE_LOG(LogCompanionAI, Log, TEXT("[Brain] 최종 응답: \"%s\""), *ReplyText);
	OnReplyReady.Broadcast(ReplyText);
}

void UCompanionBrainComponent::SaveMemory()
{
	UCompanionMemorySaveGame* SaveGameObject = Cast<UCompanionMemorySaveGame>(
		UGameplayStatics::CreateSaveGameObject(UCompanionMemorySaveGame::StaticClass()));
	if (!SaveGameObject)
	{
		return;
	}

	SaveGameObject->SavedHistory = ConversationHistory;
	SaveGameObject->MemorySummary = MemorySummary;
	UGameplayStatics::SaveGameToSlot(SaveGameObject, MemorySaveSlotName, 0);
}

void UCompanionBrainComponent::LoadMemory()
{
	if (!UGameplayStatics::DoesSaveGameExist(MemorySaveSlotName, 0))
	{
		return;
	}

	if (UCompanionMemorySaveGame* SaveGameObject = Cast<UCompanionMemorySaveGame>(
		UGameplayStatics::LoadGameFromSlot(MemorySaveSlotName, 0)))
	{
		ConversationHistory = SaveGameObject->SavedHistory;
		MemorySummary = SaveGameObject->MemorySummary;
		TrimHistory();
	}
}
