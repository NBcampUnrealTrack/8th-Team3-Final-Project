#include "Core/BGPlayerController.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Ping/BGPingComponent.h"
#include "Ping/BGPingDisplayComponent.h"
#include "Audio/BGAudioSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"   // FGameplayEventData

#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"                // FBGCardInstance
#include "Debug/BGDebug.h"
#include "UI/View/BGCardViewLibrary.h"
#include "Card/BGDataSubsystem.h"           // GetCardRow
#include "Card/BGDataSettings.h"           // EphemeralAttackCardId
#include "Card/BGGameDataTypes.h"           // FBGCardRow
#include "Card/BGDiceComponent.h"           // 4단계 판정
#include "GAS/BGAttributeSet.h"             // GetOutcomeWeightAttributes
#include "BGGameplayTags.h"           // BG::Status::*
#include "Core/BGGameState.h"               // PushToDiscard
#include "Core/BGGameMode.h"                // GetPlayMode
#include "Core/BGPlayerState.h"
#include "Lobby/BGMatchStartComponent.h"    // RequestStartPvEMatch
#include "UI/View/BGSeatViewLibrary.h"
#include "GameFramework/GameStateBase.h"      // PlayerArray
#include "UI/Presentation/BGPresentationDirector.h"
#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGSeatBoardWidget.h"
#include "UI/Widget/BGTurnPanelWidget.h"
#include "UI/Widget/BGResolutionWidget.h"
#include "UI/Widget/BGIntroRoleCardWidget.h"
#include "UI/Widget/BGMatchResultWidget.h"
#include "Core/BGEndingSequenceSet.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Core/BGCinematicCamera.h"          // 연출 카메라 (로컬 스폰)
#include "Core/BGCampfire.h"                 // 선회 중심 = 판의 중심
#include "Core/BGCharacter.h"                // 연출 주인공 캐스팅
#include "Camera/CameraShakeBase.h"          // 판정 셰이크
#include "Camera/PlayerCameraManager.h"
#include "UI/Target/BGSeatInfoDisplayComponent.h"
#include "UI/Target/BGWorldTargetingComponent.h"
#include "UI/Target/BGCameraLookComponent.h"
#include "UI/Target/BGTargetHighlight.h"
#include "Components/InputComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"

#include "BG/Public/UI/Widget/BGChatingWidget.h"
#include "BG/Public/Chating/BGChatingSubsystem.h"

//	생성자에서는 컴포넌트만 만든다 — GA 는 AbilityByCardId(BP 디폴트에서 채움)로만 조회한다.
//	하드코딩 폴백을 두지 않는 이유: 매핑이 비었을 때 엉뚱한 GA 로 조용히 새는 것보다
//	"GA 매핑 없음!" 을 화면에 띄우고 멈추는 편이 낫다(HandlePlayCard 참고).

ABGPlayerController::ABGPlayerController()
{
	Ping = CreateDefaultSubobject<UBGPingComponent>(TEXT("Ping"));
	PingDisplay = CreateDefaultSubobject<UBGPingDisplayComponent>(TEXT("PingDisplay"));

	//	전부 마우스로 하는 게임이다. 지금까지는 세션 메뉴가 켜 준 커서에 얹혀 있었는데,
	//	호버 판정이 커서에 걸리므로 남의 상태에 기대지 않고 여기서 정한다.
	bShowMouseCursor = true;

	//	둘 다 보는 사람 화면에만 있는 것이라 복제하지 않는다. 서버에서는 스스로 틱을 끈다.
	SeatInfoDisplay = CreateDefaultSubobject<UBGSeatInfoDisplayComponent>(TEXT("SeatInfoDisplay"));
	WorldTargeting  = CreateDefaultSubobject<UBGWorldTargetingComponent>(TEXT("WorldTargeting"));

	//	평상시 시점. AI 컨트롤러도 이 생성자를 거치지만, bShowMouseCursor=false/HasLocalViewer()=false 라
	//	틱에서 스스로 걸러진다(UBGCameraLookComponent::TickComponent 참고) — 따로 가를 필요 없다.
	CameraLook = CreateDefaultSubobject<UBGCameraLookComponent>(TEXT("CameraLook"));

	//	연출 큐도 화면 쪽이라 같이 복제하지 않는다.
	PresentationDirector = CreateDefaultSubobject<UBGPresentationDirector>(TEXT("PresentationDirector"));

	//	다른 WBP 를 쓰려면 BP_BGPlayerController 에서 이 값만 바꾸면 된다.
	static ConstructorHelpers::FClassFinder<UUserWidget> VictoryBP(TEXT("/Game/UI/Widget/Result/WBP_Victory"));
	if (VictoryBP.Succeeded()) { VictoryWidgetClass = VictoryBP.Class; }

	static ConstructorHelpers::FClassFinder<UUserWidget> DefeatBP(TEXT("/Game/UI/Widget/Result/WBP_Defeat"));
	if (DefeatBP.Succeeded()) { DefeatWidgetClass = DefeatBP.Class; }
}

// ======================================================================================
//  손패 UI 배선
//
//  위젯 생성과 PlayerState 복제 도착은 순서가 보장되지 않는다.
//  클라에서는 보통 위젯이 먼저 만들어지고 PlayerState 가 나중에 오지만, 리슨서버 호스트는
//  반대다. 그래서 양쪽 진입점에서 모두 TryBindHandDelegate() 를 부른다.
// ======================================================================================

void ABGPlayerController::SetHandWidget(UBGHandWidget* InHandWidget)
{
	//	위젯이 갈아끼워지면 이전 구독을 정리한다. RefreshHandWidget 은 자주 불리므로
	//	바인딩은 여기서 한 번만 한다 — 거기서 하면 갱신할 때마다 중복 구독된다.
	if (HandWidget && HandWidget != InHandWidget)
	{
		HandWidget->OnCardPlayRequested.RemoveDynamic(this, &ABGPlayerController::HandleCardPlayRequested);
		HandWidget->OnCardPlayExitStarted.RemoveDynamic(this, &ABGPlayerController::HandleCardPlayExitStarted);
	}

	HandWidget = InHandWidget;

	if (HandWidget)
	{
		HandWidget->OnCardPlayRequested.AddUniqueDynamic(this, &ABGPlayerController::HandleCardPlayRequested);
		HandWidget->OnCardPlayExitStarted.AddUniqueDynamic(this, &ABGPlayerController::HandleCardPlayExitStarted);
	}

	TryBindHandDelegate();
	TryBindAttributeDelegates();
	TryBindGameStateDelegates();
	RefreshHandWidget();
	UpdateHandInteractionLock();
}

void ABGPlayerController::SetHandInteractionLocked(bool bLocked)
{
	if (HandWidget)
	{
		HandWidget->SetInteractionLocked(bLocked);
	}
}

void ABGPlayerController::UpdateHandInteractionLock()
{
	if (!HandWidget)
	{
		return;
	}

	//	잠금 조건을 한곳에서 계산한다. 두 소스가 각자 Set 하면 서로 덮어써서
	//	"대상 선택 중인데 잠금이 풀린다" 같은 상태가 생긴다.
	const bool bSelectingTarget = IsSelectingTarget();

	//	Play 페이즈에서는 사용 횟수가 남아 있어야 조작할 수 있다(GDD §5.2).
	//	Discard 페이즈는 버리기라 한도와 무관하다.
	const ABGGameState* GS = GetBGGameState();
	const int32 MySeat = GetMySeatIndex();

	//	1회성 카드(암살자)는 한도를 쓰지 않으므로 한도가 찼어도 낼 수 있다.
	//	서버는 이미 허용하는데(HandlePlayCard) 여기서 잠가 버리면 낼 방법 자체가 없다.
	//
	//	⚠️ 잠금 단위가 손패 전체다. 그래서 이건 "1회성 카드가 있으니 손패를 열어 둔다" 이지
	//	   "아무 카드나 낼 수 있다" 가 아니다 — 한도를 쓰는 카드를 내면 서버가 거부하고
	//	   사유를 띄운다. 카드 한 장 단위로 막으려면 위젯 쪽에 per-card 잠금이 필요하다.
	bool bHasEphemeralCard = false;
	if (const ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>())
	{
		for (const FBGCardInstance& Card : MyPS->GetHand())
		{
			if (Card.bEphemeral)
			{
				bHasEphemeralCard = true;
				break;
			}
		}
	}

	const bool bCanPlay    = IsMyTurnToPlay() && GS
		&& (GS->HasCardUsesLeft(MySeat) || bHasEphemeralCard);
	const bool bCanDiscard = IsMyTurnToDiscard();

	HandWidget->SetInteractionLocked(bSelectingTarget || !(bCanPlay || bCanDiscard));
}

void ABGPlayerController::HandleCardPlayRequested(const FBGCardView& Card)
{
	if (!Card.IsValid())
	{
		return;
	}

	//	이미 다른 카드가 대상 선택 중이면 무시한다.
	//	손패 잠금이 정상 동작하면 여기까지 오지 않지만, 잠금이 걸리기 전의 입력이
	//	큐에 남아 있을 수 있어 한 번 더 막는다. 안 그러면 ContextId 가 뒤엉킨다.
	if (IsSelectingTarget())
	{
		UE_LOG(LogBGCard, Verbose, TEXT("[PC] 대상 선택 진행 중 — 카드 %d 요청 무시"), Card.InstanceId);
		return;
	}

	//	Discard 페이즈에서는 중앙 드롭이 "사용" 이 아니라 "버리기" 다.
	//	기존 드래그 UI 를 그대로 재사용하므로 버리기 전용 위젯이 필요 없다.
	if (IsMyTurnToDiscard())
	{
		ServerRequestDiscardCard(Card.InstanceId);
		return;
	}

	if (!Card.bNeedsTarget)
	{
		//	대상이 필요 없는 카드(맥주, 역마차 등) — 바로 서버로.
		RequestPlayCard(Card.InstanceId, INDEX_NONE);
		return;
	}

	//	대상이 필요한 카드 — 좌석 선택으로 넘긴다.
	BeginTargetSelectionForCard(Card.InstanceId);

	//	선택이 실제로 시작됐는지(또는 후보가 없어 즉시 끝났는지)를 보고 잠금을 다시 계산한다.
	UpdateHandInteractionLock();
}

void ABGPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (!InputComponent)
	{
		return;
	}

	//	대상 선택 취소. CancelTargetSelection 은 선택 중이 아니면 아무것도 하지 않으므로
	//	평소 우클릭에는 영향이 없다.
	//	주의: PIE 에서는 ESC 를 에디터가 먼저 먹어 세션이 종료된다 — 테스트는 우클릭으로 할 것.
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ABGPlayerController::CancelTargetSelection);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABGPlayerController::CancelTargetSelection);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &ABGPlayerController::HandleChatKeyPressed);

	//	핑 입력(좌클릭)은 발신 컴포넌트가 직접 건다. 취소는 위 우클릭·ESC 와 함께 간다.
	if (Ping)
	{
		Ping->BindInput(InputComponent);
		InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, Ping.Get(), &UBGPingComponent::CancelWheel);
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, Ping.Get(), &UBGPingComponent::CancelWheel);
	}

	//	좌클릭(대상 확정)은 조준 컴포넌트가 직접 건다.
}

void ABGPlayerController::HandleChatKeyPressed()
{
	if (!ChatWidgetInstance)
	{
		return;
	}
	
	if (ChatWidgetInstance->IsInputOpen())
	{
		return;
	}
	
	if (GFrameCounter <= ChatWidgetInstance->GetLastClosedFrame() + 1)
	{
		return;
	}
	
	ChatWidgetInstance->OpenChatInput();
}

void ABGPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라: PlayerState 가 이제야 도착했다. 여기서 구독하지 않으면 첫 손패를 놓친다.
	TryBindHandDelegate();
	TryBindAttributeDelegates();
	TryBindGameStateDelegates();
	RefreshHandWidget();
}

void ABGPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ABGPlayerController::Destroyed()
{
	//	⚠️ 파괴 뒤에 발동하면 죽은 컨트롤러를 건드린다.
	GetWorldTimerManager().ClearTimer(IntroFadeTimer);

	//	로비 복귀(ServerTravel)로 여기까지 온다 — 돌던 시퀀스와 그 액터를 두고 가면 안 된다.
	ResetEndingPresentation();

	//	판이 도는 중이면 폰을 떼어 내 월드에 남긴다. Super 가 폰을 파괴하기 전 마지막 기회다.
	const ABGGameState* GS = GetBGGameState();
	APawn* Current = GetPawn();

	if (HasAuthority() && GS && GS->IsMatchRunning() && Current)
	{
		if (ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
		{
			PS->SaveVitals(Current);
		}
		UnPossess();
	}

	Super::Destroyed();
}

void ABGPlayerController::CleanupPlayerState()
{
	//	AController::Destroyed 는 Logout 을 먼저 부른다 — 그때 붙은 이탈 표시를 여기서 본다.
	//	표시가 있으면 이 PlayerState 는 아직 판의 일부라 파괴하면 안 된다.
	ABGPlayerState* PS = Cast<ABGPlayerState>(PlayerState);
	if (PS && PS->IsDisconnected())
	{
		//	소유 관계를 끊어 둔다 — 파괴되는 컨트롤러가 끌고 가지 않도록.
		PS->SetOwner(nullptr);
		SetPlayerState(nullptr);

		UE_LOG(LogBGCard, Log, TEXT("[Seat] 좌석 %d PlayerState 유지 — 복귀 대기"),
			PS->GetSeatIndex());
		return;
	}

	Super::CleanupPlayerState();
}

void ABGPlayerController::TryBindHandDelegate()
{
	ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS || BoundPlayerState.Get() == PS)
	{
		return;   // 아직 없거나 이미 같은 PlayerState 에 구독 중
	}

	// PlayerState 가 갈아끼워진 경우(재접속 등) 이전 구독을 정리한다.
	if (ABGPlayerState* Previous = BoundPlayerState.Get())
	{
		Previous->OnHandChanged.RemoveDynamic(this, &ABGPlayerController::HandleHandChanged);
	}

	PS->OnHandChanged.AddDynamic(this, &ABGPlayerController::HandleHandChanged);
	BoundPlayerState = PS;

	UE_LOG(LogBGCard, Verbose, TEXT("[PC] 손패 델리게이트 구독 — 좌석 %d"), PS->GetSeatIndex());
}

void ABGPlayerController::SetResolutionWidget(UBGResolutionWidget* InResolutionWidget)
{
	if (ResolutionWidget)
	{
		ResolutionWidget->OnResolutionHiddenEvent.RemoveDynamic(this, &ABGPlayerController::HandleResolutionHidden);
	}
	ResolutionWidget = InResolutionWidget;

	if (ResolutionWidget)
	{
		//	등록 직후에는 보여줄 판정이 없다. 켜진 채로 시작하면 빈 패널이 화면에 남는다.
		ResolutionWidget->HideResolution();
		ResolutionWidget->OnResolutionHiddenEvent.AddUniqueDynamic(this, &ABGPlayerController::HandleResolutionHidden);
	}

	TryBindGameStateDelegates();
}

void ABGPlayerController::HandleCardPlayExitStarted(int32 InstanceId)
{
	if (ResolutionWidget)
	{
		ResolutionWidget->NotifyCardExitStarted();
	}
}

EBGResolutionTier ABGPlayerController::ToResolutionTier(EBGDiceOutcome Outcome)
{
	switch (Outcome)
	{
	case EBGDiceOutcome::CriticalFailure: return EBGResolutionTier::CriticalFailure;
	case EBGDiceOutcome::Failure:         return EBGResolutionTier::Failure;
	case EBGDiceOutcome::Success:         return EBGResolutionTier::Success;
	case EBGDiceOutcome::CriticalSuccess: return EBGResolutionTier::CriticalSuccess;
	}

	//	등급이 늘어났는데 여기를 안 고치면 화면이 UnknownTierColor(자홍색)로 뜬다 — 바로 눈에 띈다.
	return EBGResolutionTier::None;
}

void ABGPlayerController::HandleCardEquipped(int32 SeatIndex, FGameplayTag CardId)
{
	//	내 장비는 손패에 실물 위젯이 있고, 손패에서 빠질 때 그쪽이 중앙 연출을 태운다
	//	(ReleaseHeldCardsMissingFrom 폴백). 여기서 또 세우면 같은 카드가 둘 보인다.
	if (!HandWidget || SeatIndex == GetMySeatIndex())
	{
		return;
	}

	const UBGDataSubsystem* Data = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBGDataSubsystem>()
		: nullptr;

	FBGCardInstance Spectator;
	Spectator.CardId = Data ? Data->GetCardRowName(CardId) : NAME_None;
	if (Spectator.CardId.IsNone())
	{
		return;   // DT 에 없는 카드다. 연출만 빠지고 규칙은 이미 서버에서 끝났다.
	}

	//	⚠️ Tier 를 채우지 않는다. 장비는 판정을 굴리지 않으므로 등급이 없다 —
	//	   지어내면 BP 연출이 "성공" 글로우를 띄워 판정 카드와 구분되지 않는다.
	FBGCardPlayContext Context;
	Context.SeatIndex            = SeatIndex;
	Context.bResolvedWithoutRoll = true;

	HandWidget->ShowSpectatorCard(
		UBGCardViewLibrary::MakeCardView(this, Spectator), Context);
}

void ABGPlayerController::HandleCardResolved(const FBGCardResolution& Resolution)
{
	// 리슨 서버의 원격 PC에도 GameState 델리게이트가 걸릴 수 있다. UI 완료는 실제 화면만 보낸다.
	if (!IsLocalController()) return;
	ActiveResolutionPresentationId = Resolution.PresentationId;
	if (PresentationDirector && Resolution.PresentationId > LastWorldLogicFinishedId
		&& Resolution.PresentationId > 0)
	{
		if (ServerWorldHoldPresentationId != Resolution.PresentationId)
		{
			ServerWorldHold.End(PresentationDirector);
			ServerWorldHold.Begin(PresentationDirector, EBGPresentationTrack::ServerWorld);
			ServerWorldHoldPresentationId = Resolution.PresentationId;
		}
	}
	if (WorldTargeting) WorldTargeting->ClearSelectionEffectsForResolution();
	//	Row 는 두 군데서 쓰인다 — 판정 위젯의 카드 이름, 그리고 손패 연출의 스텝 목록.
	//	(카드 이름을 DT 에서 가져오는 이유: 태그 문자열을 그대로 띄우면 플레이어가 읽을 수 없다.)
	const UBGDataSubsystem* Data = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBGDataSubsystem>()
		: nullptr;
	const FBGCardRow* Row = Data ? Data->GetCardRow(Resolution.CardId) : nullptr;

	const EBGResolutionTier Tier = ToResolutionTier(Resolution.Outcome);
	
	if (GetLocalPlayer())
	{
		if (UBGChatingSubsystem* Chat = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UBGChatingSubsystem>()
			: nullptr)
		{
			const FString CardName = Row
				? Row->DisplayName.ToString()
				: Resolution.CardId.GetTagName().ToString();

			FString SeatName = FString::Printf(TEXT("좌석 %d"), Resolution.SeatIndex);
			if (const ABGGameState* GS = GetBGGameState())
			{
				if (const ABGPlayerState* SeatPS = GS->GetPlayerStateBySeat(Resolution.SeatIndex))
				{
					SeatName = SeatPS->GetPlayerName();
				}
			}
			
			const FString TierText = UBGResolutionWidget::GetTierText(Tier).ToString();

			//	대상 좌석 이름. 대상이 없는 카드(맥주 등)면 비워 둔다.
			FString TargetName;
			if (Resolution.TargetSeatIndex != INDEX_NONE)
			{
				if (const ABGGameState* GS = GetBGGameState())
				{
					if (const ABGPlayerState* TargetPS = GS->GetPlayerStateBySeat(Resolution.TargetSeatIndex))
					{
						TargetName = TargetPS->GetPlayerName();
					}
				}
			}

			const FString Line = TargetName.IsEmpty()
				 ? FString::Printf(TEXT("%s : %s"), *SeatName, *CardName)
				 : FString::Printf(TEXT("%s → %s : %s"), *SeatName, *TargetName, *CardName);

			Chat->AddSystemMessage(Resolution.bForced
			   ? FString::Printf(TEXT("%s (%s, 강제)"), *Line, *TierText)
			   : FString::Printf(TEXT("%s (%s)"), *Line, *TierText));
		}
	}

	//	── 손패 연출 통지 ──
	//	내 카드의 판정이면 중앙에 잡혀 있는 카드를 놓아 준다. 이것이 "효과가 발동했다" 의
	//	정식 신호다 — 손패에서 사라지는 것을 신호로 쓰면 등급을 알 수 없다.
	//
	//	⚠️ 아래 ResolutionWidget 조기 반환보다 먼저 와야 한다. 판정 표시 위젯을 아직
	//	   붙이지 않은 HUD 에서도 카드는 중앙에서 풀려야 하기 때문이다.
	//	맥락은 좌석과 무관하게 먼저 만든다. 손패 연출은 내 카드일 때만 쓰지만
	//	카메라 셰이크는 남의 카드에도 필요하기 때문이다.
	FBGCardPlayContext PlayContext;
	PlayContext.PresentationId = Resolution.PresentationId;
	PlayContext.Tier      = Tier;
	PlayContext.Roll      = Resolution.Roll;
	PlayContext.bForced   = Resolution.bForced;
	PlayContext.SeatIndex = Resolution.SeatIndex;

	//	서버가 실제로 쓴 비율을 그대로 %로 바꾼다. 카드 연출과 판정 위젯이 같은 값을 쓴다.
	//	⚠️ 여기서 보정을 다시 계산하면 안 된다 — 남의 어트리뷰트는 복제되지 않는다.
	UBGDiceComponent::GetOutcomeChances(Resolution.Weights,
		PlayContext.Chance.CriticalFailure, PlayContext.Chance.Failure,
		PlayContext.Chance.Success, PlayContext.Chance.CriticalSuccess);

	//	실제로 발동한 효과들을 연출 스텝으로. 이것이 연출의 정식 입력이다 —
	//	대표값 하나로 줄이면 "피해 + 뽑기" 같은 복합 카드가 뭉개진다.
	//
	//	대표 원형(Archetype)은 여기서 채우지 않는다. 카드 위젯이 자기 뷰에 이미 들고 있는
	//	값으로 메운다 — 같은 값을 두 번 유도하면 한쪽만 낡을 자리가 생긴다.
	if (Row)
	{
		PlayContext.Steps = UBGCardViewLibrary::MakeFXSteps(*Row, Tier);
	}

	if (HandWidget && Resolution.SeatIndex == GetMySeatIndex())
	{
		//	판정 결과에는 InstanceId 가 없다(카드 "종류" 만 온다). 중앙에 잡힌 카드는
		//	한 번에 하나뿐이므로 손패가 알아서 찾는다.
		HandWidget->NotifyCardResolved(PlayContext, INDEX_NONE);
	}
	else if (HandWidget && Row)
	{
		//	남이 낸 카드다 — 내 손패에는 그 카드의 위젯이 없으므로 관전용으로 하나 만들어
		//	같은 중앙 연출을 태운다. 판정 통지는 Multicast 라 전원이 여기까지 온다.
		//
		//	⚠️ 내 카드와 달리 이 위젯은 손패에 속하지 않는다. 손패에 넣으면 남이 낸 카드가
		//	   내 부채꼴에 끼어든다(ShowSpectatorCard 주석 참고).
		FBGCardInstance Spectator;
		Spectator.CardId = Data ? Data->GetCardRowName(Resolution.CardId) : NAME_None;

		if (!Spectator.CardId.IsNone())
		{
			HandWidget->ShowSpectatorCard(
				UBGCardViewLibrary::MakeCardView(this, Spectator), PlayContext);
		}
	}

	//	⚠️ 위 좌석 분기 **밖**이다. 안에 넣으면 내가 낸 카드에만 화면이 흔들려서,
	//	   같은 판을 보고 있는 다른 플레이어들만 조용한 이상한 그림이 된다.
	PlayResolveShake(PlayContext);

	if (!ResolutionWidget)
	{
		AcknowledgeResolutionPresentation();
		return;   // HUD 가 위젯을 넘기지 않았다. 판정 자체는 이미 서버에서 끝났으므로 무해하다.
	}

	FBGResolutionView View;
	View.Tier      = Tier;
	View.Roll      = Resolution.Roll;
	View.bForced   = Resolution.bForced;
	View.SeatIndex = Resolution.SeatIndex;

	View.CardName = Row
		? Row->DisplayName
		: FText::FromName(Resolution.CardId.GetTagName());

	//	카드 종류(장비/공격/마법). 손패 뷰와 같은 함수로 유도한다 —
	//	여기서 TypeTag 를 다시 읽으면 카드 UI 와 판정 UI 가 같은 카드를 다르게 그린다.
	//	Row 가 없으면 기본값(마법)이 남는다. DT 에 없는 카드라 어차피 이름도 태그로 뜬다.
	if (Row)
	{
		View.Category = UBGCardViewLibrary::DeriveCategory(*Row);
	}

	//	좌석 이름. 좌석 뷰를 거치지 않고 PlayerState 를 직접 찾는다 —
	//	좌석 보드가 없어도 판정 표시는 동작해야 한다.
	if (const ABGGameState* GS = GetBGGameState())
	{
		if (const ABGPlayerState* SeatPS = GS->GetPlayerStateBySeat(Resolution.SeatIndex))
		{
			View.SeatName = FText::FromString(SeatPS->GetPlayerName());
		}
	}

	//	위에서 뽑아 둔 값을 그대로 쓴다. 두 번 계산하면 한쪽만 고쳐질 자리가 생긴다.
	View.Chance = PlayContext.Chance;

	//	기본은 서버가 만든 자동 문구("피해 3")다 — 보너스 · 방어까지 반영된 실제 결과다.
	View.EffectText = FText::FromString(Resolution.EffectSummary);

	//	카드가 적어 둔 추가 설명. 수치(EffectText)를 덮지 않고 따로 싣는다 — 둘은 다른 칸에 뜬다.
	if (Row)
	{
		const FText& CardText = Row->OutcomeTexts.ForOutcome(Resolution.Outcome);
		if (!CardText.IsEmpty())
		{
			//	수치는 이 등급에서 실제로 발동한 효과의 합이다(피해 2 + 뽑기 1 이면 3 이 아니라 각각이지만,
			//	문구에 쓰는 것은 대표 수치 하나뿐이라 첫 효과의 값을 쓴다).
			const int32 Amount = PlayContext.Steps.IsEmpty() ? 0 : PlayContext.Steps[0].Amount;

			FFormatNamedArguments Args;
			Args.Add(TEXT("Amount"), FText::AsNumber(Amount));
			Args.Add(TEXT("Effect"), View.EffectText);

			View.DescriptionText = FText::Format(CardText, Args);
		}
	}

	ResolutionWidget->ShowResolution(View);
}

// 판정 위젯이 실제로 숨겨지면 서버에 완료를 알린다.
void ABGPlayerController::HandleResolutionHidden()
{
	AcknowledgeResolutionPresentation();
}

// 현재 판정 번호를 한 번만 서버에 보내 중복 완료 통지를 막는다.
void ABGPlayerController::AcknowledgeResolutionPresentation()
{
	const int32 PresentationId = ActiveResolutionPresentationId;
	ActiveResolutionPresentationId = 0;
	if (PresentationId > 0)
	{
		ServerConfirmResolutionHidden(PresentationId);
		if (!HandWidget || !PresentationDirector)
		{
			ServerConfirmCardPresentationFinished(PresentationId);
		}
	}
}

// 클라이언트의 UI 종료 통지를 서버 GameState에 전달한다.
void ABGPlayerController::ServerConfirmResolutionHidden_Implementation(int32 PresentationId)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->NotifyResolutionPresentationFinished(this, PresentationId);
	}
}

// 로컬 카드 연출의 모든 트랙이 끝나면 판정 번호를 서버에 전달한다.
void ABGPlayerController::HandleCardPresentationFinished(const FBGPresentationRequest& Request)
{
	if (IsLocalController() && Request.Kind == EBGPresentationKind::CardPlay
		&& Request.Context.PresentationId > 0)
	{
		ServerConfirmCardPresentationFinished(Request.Context.PresentationId);
	}
}

// 서버가 같은 판정의 클라이언트 연출 완료만 수락하도록 중계한다.
void ABGPlayerController::ServerConfirmCardPresentationFinished_Implementation(int32 PresentationId)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->NotifyCardPresentationFinished(this, PresentationId);
	}
}

// 서버의 월드 단계 종료 신호를 받으면 로컬 전체 연출의 마지막 연결 표를 놓는다.
void ABGPlayerController::ClientCardWorldLogicFinished_Implementation(int32 PresentationId)
{
	LastWorldLogicFinishedId = FMath::Max(LastWorldLogicFinishedId, PresentationId);
	if (PresentationId == ServerWorldHoldPresentationId)
	{
		ServerWorldHold.End(PresentationDirector);
		ServerWorldHoldPresentationId = 0;
	}
}

// ======================================================================================
//  판정 셰이크
//
//  셰이크 애셋을 등급별로 나누지 않는 이유: StartCameraShake 의 Scale 이 진폭 전체에
//  곱해지므로 애셋 하나로 4단계를 다 표현할 수 있다. 애셋을 나눠야 하는 것은 지속시간이나
//  파형 자체가 달라질 때뿐이다(Scale 은 진폭만 건드리고 시간에는 손대지 않는다).
//
//  셰이크는 APlayerCameraManager 가 뷰타깃 POV 를 계산한 **뒤에** 덧씌우는 모디파이어다.
//  그래서 연출 중(뷰타깃 = ABGCinematicCamera)에도 그대로 먹고, 카메라 액터는 손댈 필요가 없다.
// ======================================================================================

void ABGPlayerController::PlayResolveShake(const FBGCardPlayContext& Ctx)
{
	//	화면이 없는 쪽(데디 서버)에는 카메라 매니저가 없다. 애셋 미지정도 조용히 넘어간다 —
	//	연출이 빠지는 것뿐이고 판정 자체는 이미 끝났으므로 무해하다.
	if (!ResolveShakeClass || !PlayerCameraManager || !HasLocalViewer())
	{
		return;
	}

	const int32 TierIdx = static_cast<int32>(Ctx.Tier);
	if (!TierShakeScale.IsValidIndex(TierIdx))
	{
		//	등급이 늘었는데 배열을 안 늘린 경우다. 흔들지 않고 넘어가되 흔적은 남긴다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Shake] TierShakeScale 에 등급 %d 자리가 없다 — BP 디폴트의 배열 길이를 확인할 것."),
			TierIdx);
		return;
	}

	float Scale = TierShakeScale[TierIdx];
	if (Scale <= 0.f)
	{
		return;   // None(장비 등) — 판정하지 않은 카드는 흔들지 않는다
	}

	//	세기는 스텝의 합이 아니라 **가장 큰 한 방**으로 잡는다. 합산하면 잔효과가 많은 카드가
	//	피해 3짜리 한 방보다 더 흔들려서 체감이 뒤집힌다.
	int32 Peak = 0;
	for (const FBGCardFXStep& Step : Ctx.Steps)
	{
		if (Step.Op == EBGCardFXArchetype::Damage || Step.Op == EBGCardFXArchetype::Heal)
		{
			Peak = FMath::Max(Peak, Step.Amount);
		}
	}

	//	선형이면 피해 5 가 피해 1 의 5 배로 흔들려 화면이 무너진다. sqrt 로 눌러 둔다.
	//	(효과가 없는 카드도 등급만큼은 흔들려야 하므로 최소 1 을 보장한다.)
	Scale *= FMath::Sqrt(static_cast<float>(FMath::Max(Peak, 1)));

	PlayerCameraManager->StartCameraShake(ResolveShakeClass, FMath::Min(Scale, MaxShakeScale));
}

// ======================================================================================
//  연출 카메라
//
//  복제되는 것은 GameState 의 FBGCameraShot("무엇을 비출지") 하나뿐이다.
//  카메라 액터는 로컬이고, 뷰타깃 전환도 각 클라가 자기 화면에 대해 수행한다.
// ======================================================================================

void ABGPlayerController::EnsureCinematicCamera()
{
	//	화면이 있는 쪽에서만 필요하다. 서버가 남의 몫까지 스폰하면 쓰지도 않을 액터가 인원수만큼 생긴다.
	if (CinematicCamera || !HasLocalViewer())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = this;
	//	레벨 스트리밍·세이브에 끼어들 이유가 없는 순수 표시용 액터다.
	Params.ObjectFlags |= RF_Transient;

	CinematicCamera = World->SpawnActor<ABGCinematicCamera>(
		ABGCinematicCamera::StaticClass(), FTransform::Identity, Params);

	//	선회(Orbit)의 중심은 판의 중심 — 캠프파이어다.
	//	비워 두면 월드 원점을 축으로 돌아 테이블을 벗어난 궤도를 그린다.
	if (CinematicCamera)
	{
		if (const ABGCampfire* Campfire = ABGCampfire::Find(World))
		{
			CinematicCamera->LookAtLocation = Campfire->GetActorLocation();
		}
	}
}

void ABGPlayerController::HandleCameraShotChanged(const FBGCameraShot& Shot)
{
	// Late intro/combat camera notifications must not take the view from Sequencer.
	const ABGGameState* EndingState = GetBGGameState();
	if (EndingState && EndingState->IsEndingPresentationActive())
	{
		return;
	}

	if (!HasLocalViewer())
	{
		return;   // 화면이 없는 컨트롤러에는 뷰타깃 개념이 없다.
	}

	//	연출 속도 설정은 GameState 가 들고 있다 — 세 구간을 한곳에서 조율하기 위해서다.
	//	EditDefaultsOnly(CDO) 값이라 클라에서도 복제 없이 그대로 읽힌다.
	const ABGGameState* GS = GetBGGameState();

	if (!Shot.IsActive())
	{
		//	연출 종료 — 자기 폰으로 돌아간다. 폰이 없으면(관전·사망 대기) 손댈 것이 없다.
		if (APawn* MyPawn = GetPawn())
		{
			//	0 이면 SetViewTarget 이 즉시 전환 분기를 탄다(PlayerCameraManager 의 BlendTime 검사).
			SetViewTargetWithBlend(MyPawn,
				GS ? GS->GetCinematicExitBlendSeconds() : 0.f, VTBlend_EaseInOut, 2.f);
		}
		return;
	}

	EnsureCinematicCamera();
	if (!CinematicCamera)
	{
		return;
	}

	//	샷이 시작된 지 얼마나 지났는지 서버 시간으로 계산한다.
	//	늦게 접속한 클라는 이만큼 건너뛰고 시작해 남들과 같은 그림에 합류한다.
	//	서버 시간은 GameState 가 들고 있다(클라에서는 보정된 값이 나온다).
	const float Elapsed = GS
		? FMath::Max(0.f, GS->GetServerWorldTimeSeconds() - Shot.StartServerTime)
		: 0.f;

	const bool bAlreadyInCinematic = (GetViewTarget() == CinematicCamera);

	if (bAlreadyInCinematic)
	{
		//	이미 연출 중이다 — 뷰타깃은 그대로 두고 액터만 다음 샷으로 날린다.
		//	여기서 SetViewTarget 을 다시 부르면 카메라 매니저 블렌드가 끼어들어 튄다.
		CinematicCamera->BeginShot(Shot, Elapsed);
		return;
	}

	//	연출 진입 — 카메라를 먼저 목표에 붙여 놓고 뷰타깃을 넘긴다.
	//	순서가 반대면 카메라가 원점(0,0,0)에 있는 한 프레임이 화면에 보인다.
	CinematicCamera->SnapToShot(Shot);
	CinematicCamera->BeginShot(Shot, Elapsed);

	SetViewTargetWithBlend(CinematicCamera,
		GS ? GS->GetCinematicEnterBlendSeconds() : 0.f, VTBlend_EaseInOut, 2.f,
		/*bLockOutgoing=*/true);
}

void ABGPlayerController::TryBindAttributeDelegates()
{
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	UAbilitySystemComponent* ASC =
		PS ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PS->GetPawn()) : nullptr;

	if (!ASC || BoundASC.Get() == ASC)
	{
		return;   // 폰이 아직 없거나 이미 같은 ASC 에 구독 중
	}

	//	폰이 갈아끼워졌다(재접속·리스폰). 이전 구독을 정리하지 않으면 죽은 ASC 에 콜백이 남는다.
	if (UAbilitySystemComponent* Previous = BoundASC.Get())
	{
		for (const FGameplayAttribute& Attribute : UBGAttributeSet::GetOutcomeWeightAttributes())
		{
			Previous->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		}
	}

	for (const FGameplayAttribute& Attribute : UBGAttributeSet::GetOutcomeWeightAttributes())
	{
		ASC->GetGameplayAttributeValueChangeDelegate(Attribute)
			.AddUObject(this, &ABGPlayerController::HandleWeightAttributeChanged);
	}

	BoundASC = ASC;

	//	구독 직전에 이미 바뀐 값이 있을 수 있다(직업 GE 가 폰 스폰과 함께 붙는다).
	RefreshHandWidget();

	UE_LOG(LogBGCard, Verbose, TEXT("[PC] 판정 보정 어트리뷰트 구독 — 좌석 %d"), PS->GetSeatIndex());
}

void ABGPlayerController::HandleWeightAttributeChanged(const FOnAttributeChangeData& /*Data*/)
{
	//	어떤 보정이 바뀌었는지는 중요하지 않다 — 손패 전체의 최종 확률을 다시 계산한다.
	RefreshHandWidget();
}

void ABGPlayerController::HandleHandChanged()
{
	//	장비 장착처럼 손패와 보정이 함께 바뀌는 경우, 손패 변경이 GE 적용보다 먼저 온다.
	//	여기서 구독을 재시도해 두면 뒤이어 도착하는 보정 변경으로 확률이 정정된다.
	TryBindAttributeDelegates();

	RefreshHandWidget();
}

void ABGPlayerController::RefreshHandWidget()
{
	if (!HandWidget)
	{
		return;   // HUD 가 아직 SetHandWidget 을 부르지 않았다. 부르면 그때 다시 그린다.
	}

	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		HandWidget->ClearHand();
		return;
	}

	// 게임 로직 타입 → UI 뷰모델. 이 한 줄이 로직/UI 경계다.
	// OnCardPlayRequested 구독은 SetHandWidget 에서 한 번만 한다(여기서 하면 중복 구독).
	HandWidget->SetHand(UBGCardViewLibrary::MakeHandViews(PS));
}

// ======================================================================================
//  좌석 보드 UI 배선
//
//  좌석 정보(HP / 손패 장수 / 장비 / 생사)는 바뀌는 경로가 여럿이라 단일 델리게이트가 없다.
//  프로토타입에서는 타이머로 주기 갱신한다. SetSeats 가 좌석 번호로 위젯을 재사용하므로
//  갱신 중에도 마우스 호버나 대상 선택 상태가 끊기지 않는다.
//  TODO: 어트리뷰트 변경 델리게이트 + PlayerArray 변경 훅으로 교체하면 폴링을 없앨 수 있다.
// ======================================================================================

void ABGPlayerController::SetSeatBoardWidget(UBGSeatBoardWidget* InSeatBoard)
{
	//	위젯이 갈아끼워지면 이전 구독을 정리한다.
	if (SeatBoardWidget && SeatBoardWidget != InSeatBoard)
	{
		SeatBoardWidget->OnSeatSelected.RemoveDynamic(this, &ABGPlayerController::HandleSeatSelected);
		SeatBoardWidget->OnTargetSelectionCancelled.RemoveDynamic(this, &ABGPlayerController::HandleTargetSelectionCancelled);
	}

	SeatBoardWidget = InSeatBoard;

	if (SeatBoardWidget)
	{
		SeatBoardWidget->OnSeatSelected.AddDynamic(this, &ABGPlayerController::HandleSeatSelected);
		SeatBoardWidget->OnTargetSelectionCancelled.AddDynamic(this, &ABGPlayerController::HandleTargetSelectionCancelled);

		//	머리 위 3D 정보와 같은 것을 두 번 보여주지 않는다. HUD 애셋은 그대로 두고
		//	여기서만 접는다 — bShowSeatBoardWidget 를 켜면 예전 화면으로 돌아온다.
		if (!bShowSeatBoardWidget)
		{
			SeatBoardWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	RefreshSeatBoard();
}

void ABGPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (HasLocalViewer())
	{
		if (PresentationDirector)
		{
			PresentationDirector->OnPresentationFinished.AddUniqueDynamic(
				this, &ABGPlayerController::HandleCardPresentationFinished);
		}
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->PlayMusic(BG::Audio::Music::Gameplay.GetTag());
		}
	}
	

	//	PvE 전용 "게임 시작" 버튼은 여기서 띄우지 않는다 — WBP_BGHUD 안에 얹혀 있고,
	//	PvE 인지·로비를 건너뛸 차례인지는 UBGPvELobbyWidget 이 스스로 본다.
	//	따로 AddToViewport 하면 도움말·설정 패널 위로 버튼이 튀어나온다.
	//	PvE 보상 선택 화면도 같은 이유로 WBP_BGHUD 안에 얹혀 있다(UBGRewardSelectWidget 참고).

	//	UI 주기 갱신은 위젯 연결과 무관하게 돌린다.
	//	예전에는 SetSeatBoardWidget 안에서 타이머를 걸었는데, 그러면 좌석 보드를 연결하지
	//	않은 HUD 에서는 손패 잠금 갱신까지 함께 멈춰버린다(숨은 의존성).
	//	갱신 함수들은 위젯이 없으면 스스로 빠져나가므로 항상 돌려도 안전하다.
	if (SeatBoardRefreshInterval > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(SeatRefreshTimer, this,
				&ABGPlayerController::RefreshSeatBoard, SeatBoardRefreshInterval, /*bLoop=*/true);
		}
	}

	//	3D 조준도 좌석 보드와 같은 계약을 쓴다 — 같은 처리 함수에 붙인다.
	if (WorldTargeting)
	{
		WorldTargeting->OnSeatSelected.AddUniqueDynamic(this, &ABGPlayerController::HandleSeatSelected);
		WorldTargeting->OnTargetSelectionCancelled.AddUniqueDynamic(
			this, &ABGPlayerController::HandleTargetSelectionCancelled);
	}
}

void ABGPlayerController::CollectGameplayHudWidgets(TArray<UWidget*>& OutWidgets) const
{
	OutWidgets.Append({ HandWidget, SeatBoardWidget, TurnPanelWidget });
}

ABGGameState* ABGPlayerController::GetBGGameState() const
{
	return GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
}

void ABGPlayerController::TryBindGameStateDelegates()
{
	//	GameState 는 PlayerController 보다 늦게 복제된다 — 여러 진입점에서 재시도하고,
	//	같은 것을 다시 구독하지 않도록 여기서 한 번만 거른다.
	ABGGameState* GS = GetBGGameState();
	if (!GS || BoundGameState.Get() == GS)
	{
		return;
	}

	//	판이 갈아치워진 경우(시드 전환)다. 옛 GameState 의 구독을 끊지 않으면 통지가 겹친다.
	if (ABGGameState* Previous = BoundGameState.Get())
	{
		Previous->OnCardResolved.RemoveAll(this);
		Previous->OnCardEquipped.RemoveAll(this);
		Previous->OnCameraShotChanged.RemoveAll(this);
		Previous->OnCinematicStageChanged.RemoveAll(this);
		Previous->OnIntroStageChanged.RemoveAll(this);
	}
	ServerWorldHold.End(PresentationDirector);
	ServerWorldHoldPresentationId = 0;
	LastWorldLogicFinishedId = 0;
	ActiveResolutionPresentationId = 0;

	GS->OnCardResolved.AddUObject(this, &ABGPlayerController::HandleCardResolved);
	GS->OnCardEquipped.AddUObject(this, &ABGPlayerController::HandleCardEquipped);
	GS->OnCameraShotChanged.AddUObject(this, &ABGPlayerController::HandleCameraShotChanged);
	GS->OnCinematicStageChanged.AddUObject(this, &ABGPlayerController::HandleCinematicStageChangedForFX);
	GS->OnIntroStageChanged.AddUObject(this, &ABGPlayerController::HandleIntroStageChanged);
	GS->OnMatchEndStageChanged.AddUniqueDynamic(this, &ABGPlayerController::HandleMatchEndStageChanged);
	GS->OnTurnStateChanged.AddUniqueDynamic(this, &ABGPlayerController::HandleTurnStateChanged);

	BoundGameState = GS;

	//	⚠️ 구독보다 판이 먼저 시작됐을 수 있다(늦게 접속). 상태로 둔 것들은 지금 한 번
	//	   맞춰 줘야 첫 통지가 올 때까지 화면이 비어 있지 않다.
	HandleCameraShotChanged(GS->GetCameraShot());
	HandleIntroStageChanged(GS->GetIntroStage());
	HandleMatchEndStageChanged(GS->GetMatchEndStage());
	HandleTurnStateChanged();

	UE_LOG(LogBGCard, Verbose, TEXT("[PC] GameState 통지 구독"));
}

//	도입 연출의 화면 페이드. 야영지 진입은 검은 화면에서 밝아지며 시작한다.
//
//	⚠️ 위젯 위에 덮는 검은 판이 아니라 카메라 페이드다 — 위젯으로 가리면 그 위에 뜨는
//	   다른 UI(로딩·채팅)가 새어 나온다. 카메라 페이드는 화면 전체를 마지막에 덮는다.
void ABGPlayerController::HandleIntroStageChanged(EBGIntroStage Stage)
{
	if (!HasLocalViewer() || !PlayerCameraManager)
	{
		return;
	}

	if (Stage == EBGIntroStage::Campfire)
	{
		//	⚠️ PvP 는 맵 이동 없이 로비에서 제자리로 시작한다(UBGSessionFlow::StartGame) —
		//	   이 순간 화면은 이미 밝다. 먼저 검게 덮지 않으면 "검은 화면에서 밝아짐" 이 성립하지 않는다.
		PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, /*bInFadeAudio=*/true);

		//	장작 소리만 들리는 구간. 이만큼 머문 뒤 밝아진다.
		GetWorldTimerManager().SetTimer(IntroFadeTimer, this,
			&ABGPlayerController::BeginIntroFadeIn, FMath::Max(IntroBlackHoldSeconds, 0.01f), /*bLoop=*/false);
	}
	else
	{
		//	다른 단계에서 붙었거나 연출이 끝났는데 화면이 덮여 있으면 곧바로 걷는다.
		GetWorldTimerManager().ClearTimer(IntroFadeTimer);
		PlayerCameraManager->StopCameraFade();
	}

	UpdateIntroRoleCard(Stage);

	//	HUD 는 BP 라 네이티브 통지를 못 듣는다. 무엇을 보여줄지는 HUD 가 정한다.
	OnIntroStageChangedBP.Broadcast(Stage);
}

void ABGPlayerController::BeginIntroFadeIn()
{
	if (PlayerCameraManager)
	{
		PlayerCameraManager->StartCameraFade(1.f, 0.f, IntroFadeInSeconds, FLinearColor::Black,
			/*bShouldFadeAudio=*/true, /*bHoldWhenFinished=*/false);
	}
}

// ══════════════════════════════════════════════════════════════════════════════════════
//  엔딩 시퀀스 — 승리 진영마다 다른 성 연출
//
//  ⚠️ 각 클라가 로컬로 재생한다. 서버는 단계와 승리 진영만 복제한다 —
//     시퀀스 액터를 복제하면 재생 위치를 매 프레임 맞춰야 하고 렉에서 끊긴다.
// ══════════════════════════════════════════════════════════════════════════════════════

void ABGPlayerController::HandleMatchEndStageChanged(EBGMatchEndStage Stage)
{
	if (!HasLocalViewer())
	{
		return;
	}

	//	PvP·PvE 가 같은 흐름을 탄다. 다른 점은 PvE 조우 승리(보스 아님) 하나뿐이다 —
	//	서버가 시퀀스 단계를 건너뛰고(Victory → Credits) 결과 표 없이 곧바로 다음 조우로 간다.
	const ABGGameState* GS = GetBGGameState();
	const bool bEncounterClear = GS && GS->IsPvEEncounterClear();

	//	⚠️ 늦게 들어온 클라는 중간 단계부터 받는다 — 각 단계는 앞 단계를 거쳤다고 가정하지 않는다.
	switch (Stage)
	{
	case EBGMatchEndStage::FinalReveal:
		//	시퀀스는 승리 위젯이 떠 있는 동안까지 여유가 있다 — 지금부터 받아 둔다.
		if (!bEncounterClear)
		{
			PrepareEndingSequence();
		}
		break;

	case EBGMatchEndStage::Victory:
		if (!bEncounterClear)
		{
			if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
			{
				Audio->StopMusic(0.5f);
			}
		}
		//	HUD 는 아직 둔다 — 시퀀스로 컷될 때 걷힌다.
		if (!bEncounterClear)
		{
			PrepareEndingSequence();
		}
		ShowVictoryWidget();
		break;

	case EBGMatchEndStage::EndingSequence:
		HideGameplayWidgetsForEnding();
		HideVictoryWidget();
		PlayEndingSequence();
		break;

	case EBGMatchEndStage::Credits:
		//	조우 승리는 결과 표 없이 곧바로 로비로 간다(EncounterClearCreditsSeconds) — 걷기만 한다.
		if (bEncounterClear)
		{
			ResetEndingPresentation();
			break;
		}
		//	시퀀스는 마지막 프레임에 멈춰 있다(bPauseAtEnd) — 결과 표의 배경으로 남긴다.
		HideGameplayWidgetsForEnding();
		HideVictoryWidget();
		ShowMatchResult();
		break;

	default:
		ResetEndingPresentation();
		break;
	}
}

void ABGPlayerController::ShowVictoryWidget()
{
	if (VictoryWidgetInstance || !GetLocalPlayer())
	{
		return;
	}

	const TSubclassOf<UUserWidget> WidgetClass = DidIWin() ? VictoryWidgetClass : DefeatWidgetClass;
	if (!WidgetClass)
	{
		return;
	}

	VictoryWidgetInstance = CreateWidget<UUserWidget>(this, WidgetClass);
	if (VictoryWidgetInstance)
	{
		VictoryWidgetInstance->AddToViewport(1000);
		// 승패 UI 효과음은 시네마틱 테마와 분리해 조우 승리에서도 재생한다.
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->Play(DidIWin() ? BG::Audio::UI::MatchResult::Victory.GetTag()
				: BG::Audio::UI::MatchResult::Defeat.GetTag());
		}
	}

	//	보기만 하는 구간이다. 결과 화면이 뜨면 다시 연다.
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ABGPlayerController::HideVictoryWidget()
{
	if (VictoryWidgetInstance)
	{
		VictoryWidgetInstance->RemoveFromParent();
		VictoryWidgetInstance = nullptr;
	}
}

//	전원 정체·승패 표. 판이 끝나면 역할이 전원 공개되므로 남의 정체도 읽힌다.
void ABGPlayerController::ShowMatchResult()
{
	if (!MatchResultWidgetClass || !GetLocalPlayer())
	{
		return;   //	결과 화면을 안 지정했으면 그대로 로비 복귀를 기다린다.
	}

	if (!MatchResultWidgetInstance)
	{
		MatchResultWidgetInstance = CreateWidget<UBGMatchResultWidget>(this, MatchResultWidgetClass);
		if (!MatchResultWidgetInstance)
		{
			return;
		}
		MatchResultWidgetInstance->AddToViewport(1000);
	}

	MatchResultWidgetInstance->ShowResult();

	//	버튼을 눌러야 하므로 커서를 연다. 로비로 넘어가면 월드째 새로 열려 원래대로 돌아온다.
	bShowMouseCursor = true;
	SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
}

//	맵에 배치된 목록(ABGEndingSequenceSet)이 먼저다 — 시퀀스는 그 맵의 성을 찍은 것이라 맵마다 다르다.
//	목록이 없는 맵에서는 EndingSequences 로 돌아간다. 이쪽은 소프트 참조라 받아 둔 뒤에야 값이 나온다.
ULevelSequence* ABGPlayerController::ResolveEndingSequence() const
{
	const ABGGameState* GS = GetBGGameState();
	if (!GS)
	{
		return nullptr;
	}

	if (const ABGEndingSequenceSet* MapSet = ABGEndingSequenceSet::Find(this))
	{
		if (ULevelSequence* MapSequence = MapSet->FindSequence(GS->GetWinningRole()))
		{
			return MapSequence;
		}
	}

	const TSoftObjectPtr<ULevelSequence>* Found = EndingSequences.Find(GS->GetWinningRole());
	return Found ? Found->Get() : nullptr;
}

void ABGPlayerController::PrepareEndingSequence()
{
	const ABGGameState* GS = GetBGGameState();
	if (!GS || EndingSequenceHandle.IsValid() || ResolveEndingSequence())
	{
		return;   //	이미 받고 있거나 받아 둔 게 있다.
	}

	const TSoftObjectPtr<ULevelSequence>* Found = EndingSequences.Find(GS->GetWinningRole());
	if (!Found || Found->IsNull())
	{
		//	이 진영의 시퀀스를 안 넣었다 — 연출 없이 문구만 뜨고 지나간다.
		return;
	}

	EndingSequenceHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Found->ToSoftObjectPath(),
		FStreamableDelegate::CreateWeakLambda(this, [this]()
		{
			//	받는 동안 재생 시점이 지나갔으면 지금 튼다.
			if (bEndingSequenceWantsPlay)
			{
				PlayEndingSequence();
			}
		}));
}

void ABGPlayerController::PlayEndingSequence()
{
	bEndingSequenceWantsPlay = true;

	ULevelSequence* Sequence = ResolveEndingSequence();
	if (!Sequence)
	{
		PrepareEndingSequence();   //	아직 안 왔다. 도착하면 위 콜백이 다시 부른다.
		return;
	}

	if (EndingSequencePlayer)
	{
		return;   //	이미 돌고 있다.
	}

	//	시퀀스가 카메라를 잡게 둔다 — 성이 무너지는 동안 플레이어 시점이 살아 있으면 연출이 깨진다.
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bPauseAtEnd = true;
	Settings.bDisableLookAtInput  = true;
	Settings.bDisableMovementInput = true;

	ALevelSequenceActor* SpawnedActor = nullptr;
	EndingSequencePlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(
		this, Sequence, Settings, SpawnedActor);
	EndingSequenceActor = SpawnedActor;

	if (EndingSequencePlayer)
	{
		EndingSequencePlayer->Play();
		if (EndingSequencePlayer->IsPlaying())
		{
			PlayEndResultAudio();
		}
	}
}

void ABGPlayerController::PlayEndResultAudio()
{
	// 결과 시퀀스가 실제 시작된 뒤 내 승패 테마를 한 번만 재생한다.
	if (bEndResultAudioPlayed || !HasLocalViewer())
	{
		return;
	}
	if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
	{
		Audio->StopMusic(0.5f);
		bEndResultAudioPlayed = Audio->Play(
			DidIWin() ? BG::Audio::System::Victory.GetTag() : BG::Audio::System::Defeat.GetTag()) != nullptr;
	}
}

void ABGPlayerController::ResetEndingPresentation()
{
	if (bEndResultAudioPlayed)
	{
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->StopByTag(BG::Audio::System::Victory.GetTag(), 0.5f);
			Audio->StopByTag(BG::Audio::System::Defeat.GetTag(), 0.5f);
		}
	}
	bEndResultAudioPlayed = false;
	StopEndingSequence();
	HideVictoryWidget();

	if (MatchResultWidgetInstance)
	{
		MatchResultWidgetInstance->RemoveFromParent();
		MatchResultWidgetInstance = nullptr;
	}

	for (const TPair<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Entry : EndingHiddenWidgets)
	{
		if (UUserWidget* Widget = Entry.Key.Get())
		{
			Widget->SetVisibility(Entry.Value);
		}
	}
	EndingHiddenWidgets.Empty();
}

void ABGPlayerController::StopEndingSequence()
{
	bEndingSequenceWantsPlay = false;
	EndingSequenceHandle.Reset();

	if (EndingSequencePlayer)
	{
		EndingSequencePlayer->Stop();
		EndingSequencePlayer = nullptr;
	}

	if (EndingSequenceActor)
	{
		EndingSequenceActor->Destroy();
		EndingSequenceActor = nullptr;
	}
}

EBGIntroStage ABGPlayerController::GetIntroStage() const
{
	const ABGGameState* GS = GetBGGameState();
	return GS ? GS->GetIntroStage() : EBGIntroStage::None;
}

//	좌석 번호를 이름으로 바꾸는 것은 좌석 뷰가 이미 한다 — 여기서 또 PlayerState 를 뒤지지 않는다.
static FText FindSeatPlayerName(const APlayerController* PC, int32 Seat)
{
	if (!PC || Seat == INDEX_NONE)
	{
		return FText::GetEmpty();
	}

	const ABGPlayerState* PS = UBGSeatViewLibrary::FindPlayerStateBySeat(PC, Seat);
	return PS ? FText::FromString(PS->GetPlayerName()) : FText::GetEmpty();
}

FText ABGPlayerController::GetFirstTurnPlayerName() const
{
	const ABGGameState* GS = GetBGGameState();
	return FindSeatPlayerName(this, GS ? GS->GetFirstTurnSeat() : INDEX_NONE);
}

FText ABGPlayerController::GetHeroPlayerName() const
{
	const ABGGameState* GS = GetBGGameState();
	return FindSeatPlayerName(this, GS ? GS->GetHeroSeat() : INDEX_NONE);
}

bool ABGPlayerController::IsFirstTurnMine() const
{
	const ABGGameState* GS = GetBGGameState();
	const int32 MySeat = GetMySeatIndex();

	return GS && MySeat != INDEX_NONE && GS->GetFirstTurnSeat() == MySeat;
}

//	② 내 역할 확인 — 이 화면 주인에게만 뜨는 카드다.
//	③ 으로 넘어갈 때 내려가며, 퇴장 연출(Anim_Out)이 있으면 그것이 끝난 뒤 사라진다.
void ABGPlayerController::UpdateIntroRoleCard(EBGIntroStage Stage)
{
	//	⚠️ 카드 그림은 미리 받아 둬야 한다. 뜨는 순간에 요청하면 그림이 한 박자 늦게 채워진다.
	//	   한 단계 앞(야영지 진입)에서 위젯을 만들어 두되 화면에는 올리지 않는다.
	const bool bPrepare = Stage == EBGIntroStage::Campfire;
	const bool bShow    = Stage == EBGIntroStage::RoleReveal;

	if (!bPrepare && !bShow)
	{
		if (IntroRoleCardInstance)
		{
			IntroRoleCardInstance->HideCard();
			IntroRoleCardInstance = nullptr;
		}
		return;
	}

	if (!IntroRoleCardClass || !GetLocalPlayer())
	{
		return;   //	카드를 안 지정했으면 이 단계는 화면 없이 지나간다 — 단계 진행은 그대로다.
	}

	if (!IntroRoleCardInstance)
	{
		IntroRoleCardInstance = CreateWidget<UBGIntroRoleCardWidget>(this, IntroRoleCardClass);
		if (!IntroRoleCardInstance)
		{
			return;
		}

		//	숨긴 채로 붙인다 — 위젯이 화면에 있어야 틱이 돌고, 그래야 값이 늦게 와도 따라잡는다.
		IntroRoleCardInstance->AddToViewport();
		IntroRoleCardInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (bPrepare)
	{
		IntroRoleCardInstance->PrepareCard();
		return;
	}

	IntroRoleCardInstance->ShowCard();
}

void ABGPlayerController::HandleCinematicStageChangedForFX(EBGCinematicStage Stage, ABGCharacter* Subject)
{
	if (!PresentationDirector)
	{
		return;
	}

	//	시네마틱 트랙의 주체는 여기 하나다 — 단계가 도는 동안 표를 쥐고, None 에서 놓는다.
	//	⚠️ 단계 전환(Primary → Secondary)에서는 놓지 않는다. 표를 계속 쥐고 있으므로
	//	   중간에 화면 연출이 풀리지 않는다.
	if (Stage != EBGCinematicStage::None)
	{
		CinematicHold.Begin(PresentationDirector, EBGPresentationTrack::Cinematic);
		return;
	}

	//	여기서야 카메라가 제자리로 돌아오고 손패가 다시 열린다 — 시네마틱 트랙의 끝이다.
	CinematicHold.End(PresentationDirector);
}

void ABGPlayerController::HandleTurnStateChanged()
{
	AbortActionIfTurnLost();


	RefreshTurnPanel();
	RefreshSeatBoard();
	RefreshTurnHighlight();
	UpdateHandInteractionLock();
	RefreshHandWidget();   //	카드의 사용 가능 표시가 페이즈에 걸려 있다.
	RefreshResolutionPresentationTrack();
}

void ABGPlayerController::RefreshResolutionPresentationTrack()
{
	//	판정 UI 대기(주사위·결과 표시)가 도는 동안 표를 쥔다.
	//	⚠️ 이 구간에는 월드 연출이 아직 없다 — 여기서 안 쥐면 잡힌 트랙이 하나도 없어
	//	   연출이 시작하자마자 끝난다.
	//	대기 플래그는 복제되고 그 OnRep 이 턴 상태 통지를 쏘므로, 클라도 양 끝을 받는다.
	const ABGGameState* GS = GetBGGameState();
	if (GS && GS->IsResolutionPresentationPlaying())
	{
		ResolutionHold.Begin(PresentationDirector, EBGPresentationTrack::Resolution);
	}
	else
	{
		ResolutionHold.End(PresentationDirector);
	}
}

void ABGPlayerController::RefreshTurnHighlight()
{
	//	CustomDepth 는 복제되지 않는 순수 로컬 렌더 상태다. 리슨서버에서는 원격 클라이언트의
	//	PC 프록시도 이 델리게이트를 받으므로, 가드가 없으면 호스트 화면의 캐릭터에 남의
	//	시점 기준 하이라이트가 덧씌워진다.
	if (!HasLocalViewer())
	{
		return;
	}

	const ABGGameState* GS = GetBGGameState();
	const ABGPlayerState* ViewerState = GetPlayerState<ABGPlayerState>();
	const int32 CurrentSeat = GS ? GS->GetCurrentSeat() : INDEX_NONE;
	const int32 ViewerSeat  = ViewerState ? ViewerState->GetSeatIndex() : INDEX_NONE;

	//	내 차례에는 찍지 않는다 — 조준 하이라이트(Selectable/Aimed)도 항상 내 차례에만 켜지므로
	//	같은 스텐실 슬롯을 두고 겹칠 일이 이걸로 사라진다.
	ABGCharacter* NextCharacter = (CurrentSeat != INDEX_NONE && CurrentSeat != ViewerSeat)
		? UBGSeatViewLibrary::FindCharacterBySeat(this, CurrentSeat)
		: nullptr;

	ABGCharacter* PrevCharacter = HighlightedTurnCharacter.Get();
	if (PrevCharacter == NextCharacter)
	{
		return;
	}

	if (PrevCharacter)
	{
		BGTargetHighlight::Apply(PrevCharacter, BGTargetHighlight::EState::None);
	}
	if (NextCharacter)
	{
		BGTargetHighlight::Apply(NextCharacter, BGTargetHighlight::EState::CurrentTurn);
	}

	HighlightedTurnCharacter = NextCharacter;
}

void ABGPlayerController::AbortActionIfTurnLost()
{
	//	밧줄이 다 타서 턴이 넘어가면 서버는 그 뒤에 오는 카드 요청을 거부한다.
	//	그런데 클라는 대상 선택을 계속 열어 두고 카드도 중앙에 잡아 둔 채라, 확정도 취소도
	//	오지 않아 화면에 박힌다. 내 차례가 아니게 된 순간 여기서 스스로 접는다.
	if (IsMyTurnToPlay() || IsMyTurnToDiscard())
	{
		return;
	}

	if (IsSelectingTarget())
	{
		//	CancelTargetSelection 이 HandleTargetSelectionCancelled 로 이어져 카드를 되돌린다.
		UE_LOG(LogBGCard, Log, TEXT("[PC] 차례가 끝나 대상 선택을 접는다"));
		CancelTargetSelection();
	}

	//	서버로 보냈지만 거부당한 카드는 취소 통지가 오지 않는다 — 남아 있으면 여기서 내린다.
	//	받아들여진 카드는 손패에서 이미 빠졌으므로, 아직 손패에 있는 것만 되돌린다.
	//	이 구분이 없으면 방금 정상적으로 낸 카드가 손패로 튀었다가 다시 사라진다.
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!HandWidget || !PS)
	{
		return;
	}

	for (const int32 HeldId : HandWidget->GetHeldCardIds())
	{
		const bool bStillInHand = PS->GetHand().ContainsByPredicate(
			[HeldId](const FBGCardInstance& Card) { return Card.InstanceId == HeldId; });

		if (bStillInHand && HandWidget->ReturnHeldCardToHand(HeldId))
		{
			UE_LOG(LogBGCard, Log, TEXT("[PC] 차례가 끝나 카드 #%d 를 손패로 되돌린다"), HeldId);
		}
	}
}

void ABGPlayerController::RefreshSeatBoard()
{
	//	접어 둔 보드에는 값을 밀어넣지 않는다. 안 보이는 위젯 때문에 매 주기 전원의
	//	뷰모델을 다시 만드는 것은 낭비다.
	if (SeatBoardWidget && bShowSeatBoardWidget)
	{
		SeatBoardWidget->SetSeats(UBGSeatViewLibrary::MakeSeatViews(this));
	}

	//	좌석 보드와 같은 타이머를 쓴다 — 갱신 주기가 갈리면 "턴은 넘어갔는데 버튼은 그대로" 가 된다.
	RefreshTurnPanel();

	//	턴/페이즈 잠금도 여기서 갱신한다. 페이즈 전환(Draw→Play)은 손패 내용을 바꾸지 않아
	//	OnHandChanged 가 오지 않는다 — 폴링하지 않으면 자기 차례가 되어도 손패가 잠긴 채 굳는다.
	UpdateHandInteractionLock();

	//	이름표 켜기도 여기서 다시 본다. 좌석 배정과 페이즈 통지의 순서가 보장되지 않아
	//	늦게 들어온 사람은 HandleTurnStateChanged 를 놓칠 수 있다.

	//	늦게 도착하는 것들에 대한 구독 재시도. 둘 다 이미 걸려 있으면 즉시 빠져나온다.
	//	  · GameState — 클라에서는 PlayerState 보다 늦게 올 수 있다. 놓치면 판정이 영영 안 뜬다.
	//	  · 폰의 ASC  — 손패보다 늦게 스폰되면 보정 구독이 비어, 장비를 껴도 확률이 안 바뀐다.
	TryBindGameStateDelegates();
	TryBindAttributeDelegates();
}

// ======================================================================================
//  턴 패널 UI 배선
// ======================================================================================

void ABGPlayerController::SetTurnPanelWidget(UBGTurnPanelWidget* InTurnPanel)
{
	if (TurnPanelWidget && TurnPanelWidget != InTurnPanel)
	{
		TurnPanelWidget->OnEndTurnRequested.RemoveDynamic(this, &ABGPlayerController::HandleEndTurnRequested);
	}

	TurnPanelWidget = InTurnPanel;

	if (TurnPanelWidget)
	{
		TurnPanelWidget->OnEndTurnRequested.AddDynamic(this, &ABGPlayerController::HandleEndTurnRequested);
	}

	RefreshTurnPanel();
}

void ABGPlayerController::HandleEndTurnRequested()
{
	RequestEndTurn();
}

void ABGPlayerController::RefreshTurnPanel()
{
	if (!TurnPanelWidget)
	{
		return;
	}

	const ABGGameState* GS = GetBGGameState();
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();

	FBGTurnView View;
	View.MySeat    = GetMySeatIndex();
	View.HandCount = PS ? PS->GetHandCount() : 0;

	if (!GS)
	{
		View.StatusText = NSLOCTEXT("BG", "TurnWaiting", "대기 중...");
		TurnPanelWidget->SetTurnView(View);
		return;
	}

	View.CurrentSeat   = GS->GetCurrentSeat();
	View.DeckCount     = GS->GetDeckCount();
	View.CardsUsed     = PS ? PS->GetCardsUsedThisTurn() : 0;
	View.CardUseLimit  = GS->GetCardUseLimitForSeat(View.MySeat);
	View.bIsMyTurn     = GS->CanSeatPlayCards(View.MySeat);
	View.bIsMyDiscard  = GS->CanSeatDiscard(View.MySeat);
	View.bMatchRunning = (View.CurrentSeat != INDEX_NONE);

	//	한도가 int32 최대치면 "한도 없음"(설정 문제로 Health 를 못 읽은 상태)이므로 숨긴다.
	const int32 Limit = GS->GetHandLimitForSeat(View.MySeat);
	View.HandLimit = (Limit >= TNumericLimits<int32>::Max()) ? INDEX_NONE : Limit;

	if (View.bIsMyDiscard)
	{
		const int32 Excess = FMath::Max(0, View.HandCount - FMath::Max(0, View.HandLimit));
		View.StatusText = FText::FromString(
			FString::Printf(TEXT("손패가 많습니다 — %d 장을 버리세요 (중앙에 드롭)"), Excess));
	}
	else if (View.bIsMyTurn)
	{
		View.StatusText = (View.CardsUsed >= View.CardUseLimit)
			? NSLOCTEXT("BG", "TurnMineNoUses", "한도를 다 썼습니다")
			: NSLOCTEXT("BG", "TurnMine", "내 차례입니다");
	}
	else if (View.bMatchRunning)
	{
		//	좌석 번호는 누구인지 바로 안 읽힌다. 이름을 쓰고, 못 찾을 때만 번호로 되돌린다.
		const ABGPlayerState* TurnPS =
			UBGSeatViewLibrary::FindPlayerStateBySeat(this, View.CurrentSeat);

		const FString TurnName = TurnPS ? TurnPS->GetPlayerName() : FString();

		View.StatusText = TurnName.IsEmpty()
			? FText::FromString(FString::Printf(TEXT("좌석 %d 차례"), View.CurrentSeat))
			: FText::FromString(FString::Printf(TEXT("%s 차례"), *TurnName));
	}
	else
	{
		View.StatusText = NSLOCTEXT("BG", "TurnNotStarted", "판 시작 대기 중");
	}

	TurnPanelWidget->SetTurnView(View);
}

// ======================================================================================
//  대상 좌석 선택
// ======================================================================================

void ABGPlayerController::BeginTargetSelection(int32 ContextId, const FText& Prompt, const TArray<int32>& SelectableSeats)
{
	BeginTargetSelectionWithStyle(ContextId, Prompt, SelectableSeats, EBGTargetSelectionStyle::Basic);
}

void ABGPlayerController::BeginTargetSelectionWithStyle(int32 ContextId, const FText& Prompt,
	const TArray<int32>& SelectableSeats, EBGTargetSelectionStyle PresentationStyle)
{
	FBGTargetRequest Request;
	Request.ContextId        = ContextId;
	Request.Prompt           = Prompt;
	Request.SelectableSeats  = SelectableSeats;
	Request.bAllowCancel     = true;
	Request.PresentationStyle = PresentationStyle;

	//	월드 조준이 켜져 있으면 그쪽으로 보낸다. 두 표현이 같은 계약을 쓰므로
	//	받는 쪽(HandleSeatSelected)은 어느 쪽에서 왔는지 알 필요가 없다.
	if (WorldTargeting && WorldTargeting->IsWorldTargetingEnabled())
	{
		WorldTargeting->BeginTargeting(Request);
		return;
	}

	if (!SeatBoardWidget)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[PC] 좌석 보드가 없어 대상 선택을 시작할 수 없습니다."));
		return;
	}

	SeatBoardWidget->BeginTargetSelection(Request);
}

void ABGPlayerController::BeginTargetSelectionForCard(int32 CardInstanceId)
{
	EBGTargetSelectionStyle PresentationStyle = EBGTargetSelectionStyle::Basic;

	// InstanceId로 실제 손패 카드를 찾은 뒤 DT의 순수 연출값만 요청에 싣는다.
	// 타기팅 컴포넌트가 카드 DT를 직접 읽지 않으므로 UI 표현 계층과 카드 규칙 계층이 섞이지 않는다.
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		const FBGCardInstance* Card = PS->GetHand().FindByPredicate(
			[CardInstanceId](const FBGCardInstance& Entry)
			{
				return Entry.InstanceId == CardInstanceId;
			});

		if (Card)
		{
			const UBGDataSubsystem* Data = GetGameInstance()
				? GetGameInstance()->GetSubsystem<UBGDataSubsystem>()
				: nullptr;
			if (const FBGCardRow* Row = Data ? Data->GetCardRow(Card->CardId) : nullptr)
			{
				PresentationStyle = Row->TargetSelectionStyle;
			}
		}
	}

	//	사거리 판정은 아직 없다 — 살아 있는 다른 좌석 전부를 후보로 연다(STEP 7/8 에서 좁힌다).
	BeginTargetSelectionWithStyle(
		CardInstanceId,
		NSLOCTEXT("BG", "SelectTargetSeat", "대상을 선택하세요"),
		UBGSeatViewLibrary::GetSelectableSeats(this, /*bExcludeSelf=*/true),
		PresentationStyle);
}

void ABGPlayerController::CancelTargetSelection()
{
	//	둘 다 부른다. 진행 중이 아닌 쪽은 스스로 빠져나가므로 어느 표현이 켜져 있든 접힌다.
	if (WorldTargeting)
	{
		WorldTargeting->CancelTargeting();
	}

	if (SeatBoardWidget)
	{
		SeatBoardWidget->CancelTargetSelection();
	}
}

bool ABGPlayerController::IsSelectingTarget() const
{
	if (WorldTargeting && WorldTargeting->IsTargeting())
	{
		return true;
	}
	return SeatBoardWidget && SeatBoardWidget->IsSelectingTarget();
}

void ABGPlayerController::HandleSeatSelected(int32 SeatIndex, int32 ContextId)
{
	UE_LOG(LogBGCard, Log, TEXT("[PC] 대상 좌석 확정 — Seat=%d Context(카드 InstanceId)=%d"),
		SeatIndex, ContextId);

	UpdateHandInteractionLock();

	//	⚠️ "내가 냈다" 표시를 여기서 다시 찍는다. 드롭 시점의 표시는 좌석을 고르는 사이에
	//	   만료되고(PlayedMarkLifetime), 표시가 없으면 중앙 연출 없이 버려지듯 사라진다.
	if (HandWidget)
	{
		HandWidget->MarkCardPlayed(ContextId);
	}

	//	기본 동작: ContextId 를 카드 InstanceId 로 보고 서버에 사용 요청을 보낸다.
	//	다른 용도로 쓰려면 이 델리게이트를 구독해 분기하면 된다.
	RequestPlayCard(ContextId, SeatIndex);

	OnTargetSeatSelected.Broadcast(SeatIndex, ContextId);
}

void ABGPlayerController::HandleTargetSelectionCancelled(int32 ContextId)
{
	UE_LOG(LogBGCard, Verbose, TEXT("[PC] 대상 선택 취소 — Context=%d"), ContextId);

	//	취소되면 카드는 손패에 그대로 남는다(서버에 보내지 않았다).
	//	드롭하면서 중앙으로 올려 둔 카드를 손패로 되돌린다 — ContextId 가 곧 InstanceId 다.
	if (HandWidget)
	{
		HandWidget->ReturnHeldCardToHand(ContextId);

		//	드롭 시점에 찍힌 "내가 냈다" 표시도 거둔다. 남겨 두면 만료되기 전에 이 카드가
		//	다른 사유로 빠질 때(도둑맞기 등) 내지도 않은 카드가 중앙 연출을 탄다.
		HandWidget->ClearPlayedMark(ContextId);
	}

	//	다시 집을 수 있게 푼다.
	UpdateHandInteractionLock();

	OnTargetSelectionCancelled.Broadcast(ContextId);
}

void ABGPlayerController::RequestPlayCard(int32 InstanceId, int32 TargetSeat)
{
	// 클라(요청 발신) 확인용. 이 줄이 안 뜨면 UI 바인딩이 RequestPlayCard 를 안 부르는 것.
	BGDebug::Screen(
		FString::Printf(TEXT("[클라] 카드요청 보냄  Instance=%d  TargetSeat=%d  (→ 서버)"),
			InstanceId, TargetSeat),
		FColor(180, 180, 255), /*Time=*/5.f);

	// 로컬에서는 검증하지 않는다 (클라 표시는 힌트일 뿐, 권위는 서버). 그대로 중계.
	ServerRequestPlayCard(InstanceId, TargetSeat);
}

void ABGPlayerController::RequestPlayCardByTag(FGameplayTag CardId, int32 TargetSeat)
{
	// 손패에서 그 종류의 첫 장을 찾아 InstanceId 로 넘긴다 (테스트 편의 함수).
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS || !CardId.IsValid())
	{
		return;
	}

	for (const FBGCardInstance& Card : PS->GetHand())
	{
		if (Card.CardId == CardId.GetTagName())
		{
			RequestPlayCard(Card.InstanceId, TargetSeat);
			return;
		}
	}

	BGDebug::Screen(
		FString::Printf(TEXT("[클라] 손패에 %s 가 없습니다 — 요청 취소."), *CardId.ToString()),
		FColor::Red, /*Time=*/5.f);
}

bool ABGPlayerController::ServerRequestPlayCard_Validate(int32 InstanceId, int32 TargetSeat)
{
	// 형식 검증만 (악의적 페이로드 차단). 규칙 검증은 _Implementation 에서.
	return InstanceId != INDEX_NONE;
}

void ABGPlayerController::ServerRequestPlayCard_Implementation(int32 InstanceId, int32 TargetSeat)
{
	HandlePlayCard(InstanceId, TargetSeat);
}

// 턴 검증 우회 — GA 를 단독으로 테스트할 때만 켠다. 0 이면 정상 규칙 적용.
static int32 GBGIgnoreTurnOrder = 0;
static FAutoConsoleVariableRef CVarBGIgnoreTurnOrder(
	TEXT("BG.IgnoreTurnOrder"),
	GBGIgnoreTurnOrder,
	TEXT("1 이면 턴/페이즈 검증을 건너뛴다(GA 단독 테스트용). 기본 0."),
	ECVF_Default);

FText ABGPlayerController::GetPlayRejectText(EBGCardPlayRejectReason Reason)
{
	switch (Reason)
	{
	case EBGCardPlayRejectReason::NotYourTurn:     return NSLOCTEXT("BG", "Reject_NotYourTurn",     "지금은 당신의 차례가 아닙니다.");
	case EBGCardPlayRejectReason::NoCardUsesLeft:  return NSLOCTEXT("BG", "Reject_NoUsesLeft",      "이번 턴에 쓸 수 있는 카드를 다 썼습니다.");
	case EBGCardPlayRejectReason::TargetDead:      return NSLOCTEXT("BG", "Reject_TargetDead",      "그 대상은 이미 쓰러졌습니다.");
	case EBGCardPlayRejectReason::CardNotInHand:   return NSLOCTEXT("BG", "Reject_NotInHand",       "그 카드는 손패에 없습니다.");
	case EBGCardPlayRejectReason::AlreadyEquipped: return NSLOCTEXT("BG", "Reject_AlreadyEquipped", "이미 장착하고 있습니다.");
	case EBGCardPlayRejectReason::StatusAlreadyOn: return NSLOCTEXT("BG", "Reject_StatusOn",        "이미 같은 상태라 중첩할 수 없습니다.");
	default:                                       return NSLOCTEXT("BG", "Reject_Internal",        "카드를 사용할 수 없습니다.");
	}
}

void ABGPlayerController::RejectPlayCard(int32 InstanceId, EBGCardPlayRejectReason Reason)
{
	//	거부 사유별 상세 로그는 부르는 쪽이 이미 남긴다. 여기서는 통지가 나갔다는 것만 남긴다 —
	//	통지 없이 빠져나간 경로를 찾을 때 이 줄의 유무가 단서가 된다.
	UE_LOG(LogBGCard, Log, TEXT("[BG] 카드 #%d 거부 통지 — %s"),
		InstanceId, *GetPlayRejectText(Reason).ToString());

	ClientPlayCardRejected(InstanceId, Reason);
}

void ABGPlayerController::ClientPlayCardRejected_Implementation(int32 InstanceId,
	EBGCardPlayRejectReason Reason)
{
	if (HandWidget)
	{
		//	중앙에 올라가 있으면 손패로 내린다. 아직 손패에 있으면(대상 카드) 아무 일도 없다.
		HandWidget->ReturnHeldCardToHand(InstanceId);

		//	"내가 냈다" 표시도 거둔다 — 남겨 두면 만료되기 전에 이 카드가 다른 사유로
		//	빠질 때 내지도 않은 카드가 중앙 연출을 탄다.
		HandWidget->ClearPlayedMark(InstanceId);
	}

	//	거부됐으니 다시 집을 수 있어야 한다.
	UpdateHandInteractionLock();

	//	전용 UI 가 아직 없다 — 그때까지는 화면에 직접 띄운다.
	//	위젯이 생기면 이 줄을 지우고 OnCardPlayRejected 구독으로 옮길 것.
	BGDebug::Screen(GetPlayRejectText(Reason).ToString(), FColor::Orange, /*Time=*/3.f);

	OnCardPlayRejected.Broadcast(InstanceId, Reason);
}

void ABGPlayerController::HandlePlayCard(int32 InstanceId, int32 TargetSeat)
{
	UE_LOG(LogTemp, Log, TEXT("[BG] ServerRequestPlayCard 수신 — PC=%s Instance=%d TargetSeat=%d"),
		*GetName(), InstanceId, TargetSeat);

	//	1회성 카드(암살자)·장비(파란) 카드인가. **아래 사용 한도 게이트보다 먼저 알아야 한다** —
	//	한도를 소모하지 않는 카드가 "한도 소진" 으로 거부되면 능력이 사실상 동작하지 않는다
	//	(기본 한도가 2 라 대성공은 대개 마지막 카드에서 나온다).
	//	장비도 마찬가지다 — 설치가 한도를 쓰지 않는다면(GDD §5.3 확정) 다른 카드로 한도를
	//	다 쓴 뒤에도 장비는 여전히 낼 수 있어야 한다.
	bool bEphemeralCard = false;
	bool bEquipCard = false;
	if (const ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>())
	{
		const UGameInstance* GI = GetGameInstance();
		const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;

		for (const FBGCardInstance& Card : MyPS->GetHand())
		{
			if (Card.InstanceId == InstanceId)
			{
				bEphemeralCard = Card.bEphemeral;
				if (const FBGCardRow* Row = Data ? Data->GetCardRow(Card.CardId) : nullptr)
				{
					bEquipCard = Row->TypeTag == BG::Card::Type::Blue.GetTag();
				}
				break;
			}
		}
	}

	// ── 턴/페이즈 검증 ──
	// 클라의 UI 표시는 힌트일 뿐이므로 서버가 반드시 다시 본다.
	// TODO: 사거리(WeaponRange vs 거리)·턴당 BANG 한도(BangLimit)는 아직 없다.
	{
		const ABGGameState* GS = GetBGGameState();
		const int32 MySeat = GetMySeatIndex();

		if (GBGIgnoreTurnOrder == 0 && (!GS || !GS->CanSeatPlayCards(MySeat)))
		{
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] 지금은 좌석 %d 의 차례가 아닙니다 (현재 %d / %s). 카드 #%d 거부."),
					MySeat,
					GS ? GS->GetCurrentSeat() : INDEX_NONE,
					GS ? *GS->GetPhaseTag().ToString() : TEXT("-"),
					InstanceId),
				FColor::Orange, /*Time=*/5.f);

			//	화면 메시지는 서버 창에만 뜬다 — 거부 사유를 로그에도 남긴다.
			UE_LOG(LogBGCard, Warning, TEXT("[BG] 카드 #%d 거부 — 좌석 %d 의 차례가 아니다 (현재 %d / %s)"),
				InstanceId, MySeat, GS ? GS->GetCurrentSeat() : INDEX_NONE,
				GS ? *GS->GetPhaseTag().ToString() : TEXT("-"));
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::NotYourTurn);
		}

		//	턴당 카드 사용 한도 (GDD §5.2). 버리기는 이 경로를 타지 않으므로 영향받지 않는다.
		//	1회성 카드는 한도를 쓰지 않으므로 한도가 찼어도 낼 수 있다 — 그게 능력의 핵심이다.
		//	장비(파란) 카드도 한도를 쓰지 않는다(GDD §5.3 확정) — 아래에서도 소비하지 않는다.
		if (GBGIgnoreTurnOrder == 0 && GS && !bEphemeralCard && !bEquipCard && !GS->HasCardUsesLeft(MySeat))
		{
			const ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>();
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] 이번 턴 카드 사용 한도를 다 썼습니다 (%d / %d). 카드 #%d 거부."),
					MyPS ? MyPS->GetCardsUsedThisTurn() : 0,
					GS->GetCardUseLimitForSeat(MySeat),
					InstanceId),
				FColor::Orange, /*Time=*/5.f);

			UE_LOG(LogBGCard, Warning, TEXT("[BG] 카드 #%d 거부 — 좌석 %d 사용 한도 소진"),
				InstanceId, MySeat);
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::NoCardUsesLeft);
		}

		//	죽은 좌석은 대상이 될 수 없다.
		//
		//	클라의 대상 목록(GetSelectableSeats → CollectAliveSeats)이 이미 산 사람만 보여 주지만,
		//	그건 힌트일 뿐이다. 조작 없이도 재현된다 — 대상 선택 UI 를 열어 둔 사이에 그 좌석이
		//	다이너마이트로 죽으면, 목록이 갱신돼도 이미 잡고 있던 선택은 그대로 전송된다.
		//	막지 않으면 시체에 판정이 굴러가고 연출까지 돈 뒤 카드만 사라진다
		//	(보상은 NotifyHealthDepleted 의 bDead 걸쇠가 막으므로 이중으로 나가지는 않는다).
		//
		//	턴 순서 무시(GBGIgnoreTurnOrder)와 무관하게 항상 막는다 — 죽은 대상은 순서 문제가 아니다.
		if (TargetSeat != INDEX_NONE && GS && !GS->IsSeatAlive(TargetSeat))
		{
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] 좌석 %d 은 이미 사망했습니다. 카드 #%d 거부."),
					TargetSeat, InstanceId),
				FColor::Orange, /*Time=*/5.f);

			UE_LOG(LogBGCard, Warning, TEXT("[BG] 카드 #%d 거부 — 대상 좌석 %d 은 사망"),
				InstanceId, TargetSeat);
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::TargetDead);
		}
	}

	// 시전자의 ASC 를 얻는다 (현재 ASC 는 캐릭터 소유).
	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] HandlePlayCard: 시전자 ASC 없음 (Pawn=%s)."),
			GetPawn() ? *GetPawn()->GetName() : TEXT("null"));
		return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::InternalError);
	}

	// ── InstanceId → 손패에서 카드 조회 → 카드 종류 태그 + 메커니즘 태그 ──
	// TODO(2단계): 손패에 없으면 거부는 이미 하고 있고, 실제 사용 시
	//              RemoveCardFromHand 로 그 인스턴스를 빼는 처리가 남았다.
	FGameplayTag CardId;    // 맵 조회 키 (Card.Id.*)
	FGameplayTag EventTag;  // 페이로드/로그용 메커니즘 태그 (Ability.*)
	const FBGCardRow* CardRow = nullptr;   // 판정 데이터 (OutcomeWeights / OutcomeEffects)
	FName CardCardId = NAME_None;            // DT Row 이름 (장비 중복 검사용)
	//	bEphemeralCard 는 함수 앞에서 이미 구했다 — 사용 한도 게이트가 그 값을 먼저 봐야 하기 때문이다.
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		const UGameInstance* GI = GetGameInstance();
		const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;

		for (const FBGCardInstance& Card : PS->GetHand())
		{
			if (Card.InstanceId == InstanceId)
			{
				if (const FBGCardRow* Row = Data ? Data->GetCardRow(Card.CardId) : nullptr)
				{
					CardId     = Row->CardIdTag;
					EventTag   = Row->AbilityEventTag;
					CardRow    = Row;        //	판정 비율·등급별 수치를 아래에서 읽는다
					CardCardId = Card.CardId; //	장비 중복 검사용 Row 이름
				}
				break;
			}
		}
	}

	if (!CardId.IsValid())
	{
		BGDebug::Screen(
			FString::Printf(TEXT("[서버] 카드 Instance=%d 를 손패에서 못 찾음(또는 Row 의 CardIdTag 비어있음)."),
				InstanceId),
			FColor::Red, 8.f);

		UE_LOG(LogBGCard, Warning, TEXT("[BG] 카드 #%d 거부 — 손패에 없거나 CardIdTag 가 비었다"), InstanceId);
		return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::CardNotInHand);
	}

	// ── CardId 태그 → GA 클래스 (맵을 "카드 종류"로 직접 조회) ──
	//  주의: 맵(AbilityByCardId)의 키는 카드 태그(Card.Id.*)다 — 유저 설정에 맞춰 카드 기준 조회.
	// (메커니즘 태그 Ability.* 로 거치지 않는다.)
	const TSubclassOf<UGameplayAbility>* Found = AbilityByCardId.Find(CardId);
	if (!Found || !*Found)
	{
		// 진단: 런타임에 맵에 실제로 뭐가 들었는지 그대로 찍는다.
		//   맵(0) 비어있음   → 빌드/BP 미반영
		//   키는 있는데 값 None → BP 에서 GA 클래스를 안 골랐음
		//   키가 다름          → 이 카드 태그가 맵에 없음
		FString Dump;
		for (const TPair<FGameplayTag, TSubclassOf<UGameplayAbility>>& Pair : AbilityByCardId)
		{
			Dump += FString::Printf(TEXT("%s=%s; "),
				*Pair.Key.ToString(),
				Pair.Value ? *Pair.Value->GetName() : TEXT("None"));
		}
		BGDebug::Screen(
			FString::Printf(TEXT("[서버] GA 매핑 없음! 카드 %s 를 맵에서 못 찾음.  현재 맵(%d개): %s"),
				*CardId.ToString(), AbilityByCardId.Num(),
				Dump.IsEmpty() ? TEXT("(비어있음)") : *Dump),
			FColor::Red, 10.f);

		UE_LOG(LogBGCard, Warning, TEXT("[BG] 카드 #%d 거부 — %s 의 GA 매핑 없음 (맵 %d개)"),
			InstanceId, *CardId.ToString(), AbilityByCardId.Num());
		return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::InternalError);
	}
	TSubclassOf<UGameplayAbility> AbilityClass = *Found;

	// ── 중첩 차단 (Additional-Card-List: 중첩 사용 불가) ──
	//	이 카드가 어느 등급에서든 부여하는 Status.* 를 받는 쪽이 이미 갖고 있으면 사용 자체를 막는다.
	//	카드를 소비한 뒤에 막으면 카드만 날리게 되므로 반드시 소비 "전" 에 검사한다.
	//	검사 목록을 DT 에 따로 두지 않고 효과 목록에서 유도한다 — 두 곳에 적으면 어긋난다.
	if (CardRow)
	{
		const ABGPlayerState* SelfPS   = GetPlayerState<ABGPlayerState>();
		const ABGPlayerState* TargetPSCheck = nullptr;
		if (const APawn* TargetPawn = Cast<APawn>(ResolveTargetActor(TargetSeat)))
		{
			TargetPSCheck = TargetPawn->GetPlayerState<ABGPlayerState>();
		}

		const TArray<FBGCardEffect>* AllTiers[] = {
			&CardRow->OutcomeEffects.CriticalFailure, &CardRow->OutcomeEffects.Failure,
			&CardRow->OutcomeEffects.Success,         &CardRow->OutcomeEffects.CriticalSuccess };

		for (const TArray<FBGCardEffect>* Tier : AllTiers)
		{
			for (const FBGCardEffect& Effect : *Tier)
			{
				if (Effect.Op != EBGCardEffectOp::ApplyStatus || !Effect.StatusTag.IsValid())
				{
					continue;
				}

				const ABGPlayerState* Recipient = Effect.bToTarget ? TargetPSCheck : SelfPS;
				if (Recipient && Recipient->HasPendingStatus(Effect.StatusTag))
				{
					BGDebug::Screen(
						FString::Printf(TEXT("[서버] %s 는 이미 %s 상태입니다 — 중첩할 수 없어 사용 거부."),
							Effect.bToTarget ? TEXT("대상") : TEXT("본인"),
							*Effect.StatusTag.GetTagName().ToString()),
						FColor::Orange, /*Time=*/5.f);
					return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::StatusAlreadyOn);
				}
			}
		}
	}

	// ── 대상/시전자 페이로드 구성 (뱅처럼 TargetSeat 이 있는 카드는 대상 폰을 넘긴다) ──
	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetPawn();

	//	어느 카드가 발동했는지 GA 가 알 수 있게 Card.Id.* 를 함께 실어 보낸다.
	//	범용 실행기(UGA_BGCardEffects)가 이 태그로 효과 목록을 조회한다.
	//	Resolution.* 와 같은 컨테이너를 쓰지만 접두사가 달라 충돌하지 않는다.
	Payload.InstigatorTags.AddTag(CardId);
	if (AActor* TargetActor = ResolveTargetActor(TargetSeat))
	{
		Payload.Target = TargetActor;
	}

	// ── 파란 카드(장비): 판정 없이 즉시 장착 (GDD §6.3) ──
	//	장비는 지속 효과라 판정 대상이 아니다. GA 도 발동하지 않는다.
	//	주의: 카드가 버린 패로 가지 않는다 — 손패에서 장비로 "이동" 한다.
	// 그래서 ConsumeCard(버리기)를 쓰지 않고 직접 옮긴다.
	if (CardRow->TypeTag == BG::Card::Type::Blue.GetTag())
	{
		ABGPlayerState* EquipPS = GetPlayerState<ABGPlayerState>();
		if (!EquipPS)
		{
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::InternalError);
		}

		if (EquipPS->HasEquippedCard(CardCardId))
		{
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] %s 는 이미 장착 중입니다 — 중복 장착 불가."),
					*CardRow->DisplayName.ToString()),
				FColor::Orange, /*Time=*/5.f);
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::AlreadyEquipped);
		}

		FBGCardInstance Moved;
		if (!EquipPS->RemoveCardFromHand(InstanceId, Moved))
		{
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::CardNotInHand);
		}

		if (!EquipPS->EquipCard(Moved, CardRow->EquipEffect))
		{
			//	장착 실패 — 카드를 손패로 되돌린다(카드만 사라지면 안 된다).
			EquipPS->AddCardToHand(Moved);
			return RejectPlayCard(InstanceId, EBGCardPlayRejectReason::InternalError);
		}

		//	장비 설치는 카드 사용 횟수를 소비하지 않는다(GDD §5.3 확정).
		//	위 사용 한도 게이트도 bEquipCard 로 장비를 함께 면제하므로, 다른 카드로 한도를
		//	다 쓴 뒤에도 장비는 계속 설치할 수 있다.

		BGDebug::Screen(
			FString::Printf(TEXT("장착  %s → %s"),
				*CardRow->DisplayName.ToString(),
				GetPawn() ? *GetPawn()->GetName() : TEXT("?")),
			FColor(120, 200, 255), /*Time=*/5.f);

		//	장비는 GA 도 주사위도 없이 여기서 사용 성공이 확정된다. 그래서 판정 카드가 쓰는
		//	Multicast_CardResolved 통지를 탈 수 없고, 필요한 두 가지를 여기서 직접 부른다.
		//
		//	⚠️ 아래 둘은 **대안이 아니라 서로 다른 일**이다. 하나만 남기면 그쪽이 통째로 사라진다.
		//	     Multicast_CardEquipped       — UI. 남의 화면에 관전 카드를 세운다.
		//	     PlayResolvedCardPresentation — 월드. Niagara · GameplayCue 를 재생한다.
		//	   합쳐 놓고 보면 당연하지만, 각자 다른 브랜치에서 같은 자리에 들어와 충돌했다.
		if (ABGGameState* MutableGS = GetBGGameState())
		{
			//	이게 없으면 남의 화면에서는 장비가 아무 연출 없이 장비창에만 슬쩍 나타난다.
			MutableGS->Multicast_CardEquipped(EquipPS->GetSeatIndex(), CardId);

			//	갈색 카드와 같은 연출 입구를 쓴다. DT_BGCardFX 에 Row 가 없으면 조용히 넘어가므로
			//	장착 연출을 아직 안 만든 장비도 그대로 동작한다.
			FBGCardResolution EquipResolution;
			EquipResolution.CardId    = CardId;
			EquipResolution.SeatIndex = EquipPS->GetSeatIndex();
			MutableGS->PlayResolvedCardPresentation(
				Cast<ABGCharacter>(GetPawn()), nullptr, EquipResolution);
		}
		return;
	}

	//	판정 결과를 아래 "대비" 처리에서 쓴다(블록 밖에서 보이도록 선언).
	EBGDiceOutcome ResolvedOutcome = EBGDiceOutcome::Success;
	bool bResolved = false;

	//	판정 결과 1건. 전원 통지(Multicast_CardResolved)와 연출(BeginCardCinematic)이 같은 값을 쓴다 —
	//	따로 채우면 화면에 뜬 결과와 애니메이션의 성패가 어긋날 수 있다.
	//	Roll 이 INDEX_NONE 으로 남아 있으면 "판정을 굴리지 않은 카드" 라는 뜻이다.
	FBGCardResolution Resolution;
	Resolution.CardId    = CardId;
	Resolution.SeatIndex = GetPlayerState<ABGPlayerState>()
		? GetPlayerState<ABGPlayerState>()->GetSeatIndex()
		: INDEX_NONE;

	// ── 4단계 판정 (GDD §4.2 / §9.1) ──
	//	판정은 서버가 여기서 한 번만 한다. GA 는 "결과 실행" 만 담당한다 —
	//	GA 마다 각자 굴리면 중복 판정과 "같은 판정을 두 번 적용" 위험이 생긴다(GDD §9.2).
	//	결과는 Resolution.* 태그로, 등급별 수치는 EventMagnitude 로 실어 보낸다.
	{
		UBGDiceComponent* Dice = UBGDiceComponent::Get(this);
		const ABGGameState* GS = GetBGGameState();

		if (Dice && CardRow)
		{
			//	카드 DT 의 4단계 비율에 "장비·상태의 비율 보정" 만 반영해 판정한다(GDD §4.2 / §8).
			//
			//	행운(Luck)은 반영하지 않는다 — DT 비율이 곧 표시 확률이어야 §10 확률 표시와
			//	§11 "설정 확률 vs 실측" 비교가 성립한다. (ApplyModifiers 안에서 제외됨)
			//	→ UBGDiceComponent::ApplyLuck 은 선언만 남고 호출처가 없다(§13.1 비활성).
			//
			//	주의: 장비 보정은 반드시 통과시켜야 한다. 이 호출을 빼면 드워프 장갑·마녀의 부적이
			// GE 로 어트리뷰트를 바꿔도 판정이 읽지 않아 효과가 사라진다.
			// GDD §10 도 "장비 효과가 적용된 최종 확률" 을 표시하라고 요구한다.
			//	⚠️ 표시 쪽(UBGCardViewLibrary::MakeCardView)과 같은 방법으로 구한다.
			//	   갈리면 손패에 뜬 확률과 실제 판정이 어긋난다.
			const bool bIsAttackCard = CardRow->TraitTags.HasTag(BG::Card::Trait::Attack.GetTag());

			const FBGOutcomeWeights Weights =
				Dice->ApplyModifiers(CardRow->OutcomeWeights, ASC, bIsAttackCard);

			int32 Roll = INDEX_NONE;
			EBGDiceOutcome Outcome = Dice->RollOutcome(Weights, Roll);

			//	강제되면 굴린 눈과 결과가 어긋난다. UI 가 "눈 3 → 대성공" 을 그대로 띄우면
			//	버그로 보이므로, 강제 여부를 함께 전달해 눈 대신 "판정 강제됨" 을 보여주게 한다.
			bool bForcedOutcome = false;

			//	── "다음 1회" 판정 강제 (준비 / 함정) ──
			//	상태를 소모해 굴린 결과를 덮어쓴다. 굴림 자체는 먼저 소비해 난수 수열을
			//	일정하게 유지한다(강제 여부에 따라 이후 판정이 밀리지 않도록).
			if (ABGPlayerState* CasterPS = GetPlayerState<ABGPlayerState>())
			{
				//	우선순위: 강한 강제(대성공/대실패)를 먼저 본다.
				struct FForceRule { FGameplayTag Tag; EBGDiceOutcome Forced; };
				const FForceRule Rules[] = {
					{ BG::Status::NextCard::ForceCriticalSuccess.GetTag(), EBGDiceOutcome::CriticalSuccess },
					{ BG::Status::NextCard::ForceCriticalFailure.GetTag(), EBGDiceOutcome::CriticalFailure },
					{ BG::Status::NextCard::ForceSuccess.GetTag(),         EBGDiceOutcome::Success },
					{ BG::Status::NextCard::ForceFailure.GetTag(),         EBGDiceOutcome::Failure },
				};

				for (const FForceRule& Rule : Rules)
				{
					int32 Unused = 0;
					if (!CasterPS->HasPendingStatus(Rule.Tag))
					{
						continue;
					}

					//	함정 예외 규칙(Additional-Card-List): 강제할 등급의 비율이 0 인 카드에는
					//	강제가 통하지 않는다. 그래도 상태는 소모된다 — "효과를 없앨 수 있음".
					const bool bTierPossible =
						(Rule.Forced == EBGDiceOutcome::CriticalSuccess && Weights.CriticalSuccess > 0) ||
						(Rule.Forced == EBGDiceOutcome::Success         && Weights.Success > 0) ||
						(Rule.Forced == EBGDiceOutcome::Failure         && Weights.Failure > 0) ||
						(Rule.Forced == EBGDiceOutcome::CriticalFailure && Weights.CriticalFailure > 0);

					CasterPS->ConsumePendingStatus(Rule.Tag, Unused);

					if (bTierPossible)
					{
						Outcome = Rule.Forced;
						bForcedOutcome = true;
						BGDebug::Screen(
							FString::Printf(TEXT("[강제] %s → 판정을 %s 로 고정"),
								*Rule.Tag.GetTagName().ToString(),
								*UBGDiceComponent::GetOutcomeText(Rule.Forced).ToString()),
							FColor(200, 120, 255), /*Time=*/5.f);
					}
					else
					{
						BGDebug::Screen(
							FString::Printf(TEXT("[강제] %s 는 이 카드에 통하지 않습니다(해당 등급 비율 0) — 상태만 소모."),
								*Rule.Tag.GetTagName().ToString()),
							FColor(160, 160, 160), /*Time=*/5.f);
					}
					break;   // 강제는 한 번만 적용한다
				}
			}

			Payload.InstigatorTags.AddTag(UBGDiceComponent::OutcomeToTag(Outcome));

			//	EventMagnitude 는 싣지 않는다. 등급별 수치를 담던 OutcomeMagnitudes 를 없앴고,
			//	읽던 전용 GA(GA_Bang / GA_Heal / GA_StealOrDiscard)도 전부 비활성이다.
			//	수치는 카드 효과 목록(OutcomeEffects)이 들고 있고 UGA_BGCardEffects 가 실행한다.
			ResolvedOutcome = Outcome;
			bResolved = true;

			float CF = 0.f, F = 0.f, S = 0.f, CS = 0.f;
			UBGDiceComponent::GetOutcomeChances(Weights, CF, F, S, CS);

			BGDebug::Screen(
				FString::Printf(TEXT("[판정] %s → %s  (눈 %d / 확률 대실패%.0f%% 실패%.0f%% 성공%.0f%% 대성공%.0f%%)"),
					*CardId.ToString(),
					*UBGDiceComponent::GetOutcomeText(Outcome).ToString(),
					Roll, CF, F, S, CS),
				(Outcome == EBGDiceOutcome::CriticalSuccess) ? FColor(255, 140, 0) :
				(Outcome == EBGDiceOutcome::Success)         ? FColor::Yellow :
				(Outcome == EBGDiceOutcome::Failure)         ? FColor(160, 160, 160) :
				                                                 FColor::Red,
				/*Time=*/6.f);

			//	전원에게 판정 결과를 알린다 (GDD §4.1 "모든 플레이어에게 동일한 결과를 표시").
			//	위의 BGDebug::Screen 은 서버 화면에만 찍히므로 클라는 아무것도 보지 못한다.
			Resolution.Outcome = Outcome;
			Resolution.Roll    = Roll;
			Resolution.bForced = bForcedOutcome;
			Resolution.Weights = Weights;

		}
		else if (!Dice)
		{
			//	판정 없이 GA 를 발동하면 GA 가 자체 기본값으로 폴백한다(과거 동작).
			UE_LOG(LogBGCard, Warning, TEXT("[BG] BGDiceComponent 없음 — 판정 없이 진행합니다."));
		}
	}

	BGDebug::Screen(
		FString::Printf(TEXT("[서버] 카드처리  %s #%d  메커니즘[%s] → %s  대상:%s"),
			*CardId.ToString(),
			InstanceId,
			EventTag.IsValid() ? *EventTag.ToString() : TEXT("-"),
			*AbilityClass->GetName(),
			Payload.Target.Get() ? *Payload.Target->GetName() : TEXT("없음")),
		FColor(180, 180, 255), /*Time=*/5.f);

	// 카드 주도 발동 (md §1.1): 서버 권위로 즉석 부여+1회 발동. 페이로드가 GA 의 TriggerEventData 로 전달된다.
	FGameplayAbilitySpec Spec(AbilityClass, /*Level=*/1, /*InputID=*/INDEX_NONE, /*SourceObject=*/this);
	//	사용 횟수를 발동 "전" 에 센다.
	//	GA 안의 "카드 사용한도 회복" 효과가 이 카드의 소비를 되돌릴 수 있어야 하는데,
	//	발동 후에 세면 회복이 아직 0 인 값에 작용해 아무 효과가 없어진다.
	//	⚠️ 1회성 카드(암살자)는 사용 한도를 소모하지 않는다. 여기서 세지 않으므로
	//	   아래 발동 실패 롤백과 대비·무투가 환급도 모두 건너뛴다 — 되돌릴 것이 없다.
	ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>();
	//	대비는 GA 안에서 KeepCardUse를 새로 부여한다. 발동 뒤 현재 상태만 검사하면
	//	그 대비 카드 자신을 "다음 카드"로 잘못 소비하므로, 발동 전 보유 여부를 고정해 둔다.
	const bool bHadKeepCardUseBeforeActivation = MyPS
		&& MyPS->HasPendingStatus(BG::Status::NextCard::KeepCardUse.GetTag());
	if (MyPS && !bEphemeralCard)
	{
		MyPS->IncrementCardsUsedThisTurn();
	}

	//	낸 카드는 GA 가 도는 동안 **아직 손패에 있다**(아래 ConsumeCard 는 발동 뒤에 온다).
	//	그사이 남의 손패를 건드리는 효과 — 지금은 도둑 강탈 — 가 이 카드를 집어갈 수 있고,
	//	집어가면 카드가 도둑 손으로 복제된 뒤 ConsumeCard 가 실패해 버린 패로 가지 않는다.
	//	발동은 동기로 끝나므로 이 창 밖에서는 강탈이 일어날 수 없다. 딱 이 구간만 표시한다.
	if (MyPS)
	{
		MyPS->CardInFlightInstanceId = InstanceId;
		MyPS->LastCardEffectSummary.Reset();   // 앞 카드의 결과가 이 카드 통지에 섞이지 않게
	}

	ABGGameState* PresentationGS = GetBGGameState();
	Resolution.PresentationId = PresentationGS ? PresentationGS->BeginCardRulesPresentation() : 0;
	const FGameplayAbilitySpecHandle ActivatedHandle = ASC->GiveAbilityAndActivateOnce(Spec, &Payload);
	if (PresentationGS)
	{
		Resolution.EquipmentProcs = PresentationGS->EndCardRulesPresentation(ActivatedHandle.IsValid());
	}

	if (MyPS)
	{
		MyPS->CardInFlightInstanceId = INDEX_NONE;

		//	발동은 동기라 여기서 효과가 이미 끝났다. 최종 피해는 GA 만 알므로 거기서 받아 싣는다.
		Resolution.EffectSummary = MyPS->LastCardEffectSummary;
	}

	// GiveAbilityAndActivateOnce 는 발동에 실패하면 ClearAbility 후 무효 핸들을 돌려준다
	// (AbilitySystemComponent_Abilities.cpp: InternalTryActivateAbility 실패 분기).
	// 즉 핸들 유효성이 곧 "발동 성공" 이다. 실패한 카드는 소비하지 않는다 —
	// GA 매핑 오류나 ActivationBlockedTags(State.Dead 등)로 카드가 조용히 사라지면 추적이 어렵다.
	if (!ActivatedHandle.IsValid())
	{
		//	발동 실패 — 미리 센 사용 횟수를 되돌린다(애초에 세지 않은 1회성 카드는 제외).
		if (MyPS && !bEphemeralCard)
		{
			MyPS->DecrementCardsUsedThisTurn();
		}

		BGDebug::Screen(
			FString::Printf(TEXT("[서버] %s 발동 실패 — 카드 #%d 는 손패에 그대로 둡니다."),
				*AbilityClass->GetName(), InstanceId),
			FColor::Red, /*Time=*/8.f);
		return;
	}

	// ── 연출 (아트 기획서 "전투 연출") ──────────────────────────────
	//	맨 마지막에 시작한다. 판정·효과 적용·카드 소비가 모두 끝난 뒤라야
	//	연출이 규칙에 관여하지 않는다는 원칙(Card.Trait.Cinematic 주석)이 지켜진다.
	//	연출이 켜지든 말든 결과는 완전히 동일하다.
	//
	//  GA 발동 성공이 확정됐고, 아직 ConsumeCard가 손패를 갱신하기 전인 지점에서
	//  먼저 GameState에 판정 결과를 넘기고 소유 클라이언트의 중앙 카드가 실제 등급으로
	//  ReleaseHold되고 주사위/판정 UI 애니메이션을 시작한다.
	//
	//	GameState가 DT_BGCardFX와 Card.Trait.Cinematic을 보고
	//  단발 Cue 또는 시네마틱 Primary/Secondary Cue를 선택한다. 여기서는 연출을 직접 제어하지 않는다.
	//  판정 UI 통지와 월드 연출 타이머만 열고, 실제 GameplayCue는 ResolutionPresentationSeconds 뒤에 시작한다.
	if (ABGGameState* MutableGS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		MutableGS->PlayResolvedCardPresentation(
			Cast<ABGCharacter>(GetPawn()),
			Cast<ABGCharacter>(ResolveTargetActor(TargetSeat)),
			Resolution);
	}

	// 뱅 규칙: 낸 카드는 결과와 무관하게 버린 패로 간다.
	// (뱅!이 빗나가도 그 뱅! 카드는 돌아오지 않는다)
	ConsumeCard(InstanceId);

	//	카드 사용이 확정됐다 — 밧줄은 GameState 가 다시 채운다.
	if (ABGGameState* PlayedGS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		PlayedGS->NotifyCardPlayed(GetMySeatIndex());
	}

	//	사용 횟수는 발동 전에 이미 셌다(위 참조). 판정 실패도 소비된다 (GDD §5.3) —
	//	실패 여부는 GA 안에서 갈리므로 여기서는 아무 조건도 보지 않는다.
	//	── 대비(Status.NextCard.KeepCardUse) ──
	//	실패·대실패했다면 이 카드의 사용 횟수를 되돌린다. 상태는 결과와 무관하게 소모된다
	//	("다음에 사용하는 카드" 를 이미 썼으므로).
	//	이 카드의 사용 횟수를 이미 되돌렸는가. 대비와 무투가가 겹칠 수 있는데, 둘 다 환급하면
	//	카드 한 장으로 사용한도를 두 번 돌려받는다.
	//	1회성 카드는 사용한도를 쓰지 않았으므로 환급 자체가 성립하지 않는다. 걸쇠를 미리 닫아
	//	대비·무투가 양쪽을 건너뛴다 — 그냥 두면 이 턴에 낸 **다른** 카드의 사용횟수가 환급된다.
	bool bCardUseRefunded = bEphemeralCard;

	if (MyPS && !bCardUseRefunded && bResolved && bHadKeepCardUseBeforeActivation)
	{
		int32 Unused = 0;
		MyPS->ConsumePendingStatus(BG::Status::NextCard::KeepCardUse.GetTag(), Unused);

		const bool bFailed = (ResolvedOutcome == EBGDiceOutcome::Failure
			|| ResolvedOutcome == EBGDiceOutcome::CriticalFailure);

		if (bFailed && MyPS->DecrementCardsUsedThisTurn(1) > 0)
		{
			bCardUseRefunded = true;
			BGDebug::Screen(TEXT("[대비] 실패했지만 카드 사용한도를 잃지 않았습니다."),
				FColor(120, 220, 160), /*Time=*/5.f);
		}
	}

	//	── 암살자 (EphemeralCardOnCritical) ──
	//	대성공을 낼 때마다 1회성 공격 카드를 받는다. 판정을 굴린 카드에만 해당한다 —
	//	장비처럼 판정하지 않은 카드는 bResolved 가 false 다.
	//
	//	⚠️ 1회성 카드로 낸 대성공도 또 준다(연쇄). 손패 상한이 걸려 있어 무한히 쌓이지는 않지만,
	//	   연쇄를 막고 싶으면 여기에 !bEphemeralCard 를 추가하면 된다.
	if (MyPS && bResolved && ResolvedOutcome == EBGDiceOutcome::CriticalSuccess && ASC)
	{
		const int32 GrantCount = FMath::RoundToInt(
			ASC->GetNumericAttribute(UBGAttributeSet::GetEphemeralCardOnCriticalAttribute()));

		if (GrantCount > 0)
		{
			if (ABGGameState* GS = GetBGGameState())
			{
				//	어떤 카드를 주는지도, 그게 일회용인지도 여기서 정하지 않는다 —
				//	카드는 설정이, 일회용 여부는 DT 의 Card.Trait.Ephemeral 이 정한다.
				const FGameplayTag GrantId = GetDefault<UBGDataSettings>()->EphemeralAttackCardId;
				const int32 Granted = GS->GrantCards(MyPS, GrantId, GrantCount);

				if (Granted > 0)
				{
					BGDebug::Screen(
						FString::Printf(TEXT("[암살자] 대성공 — 1회성 카드 %d장 획득"), Granted),
						FColor(200, 170, 120), /*Time=*/5.f);
				}
			}
		}
	}

	//	── 무투가 (AttackKeepCardUseChance) ──
	//	공격 카드를 썼을 때 일정 확률로 사용한도를 아낀다. 대비와 달리 **판정 결과를 보지 않는다** —
	//	성공해도 아낀다. 굴림은 방어 판정(요정 갑옷)과 같은 판정 스트림을 쓴다.
	if (MyPS && !bCardUseRefunded && CardRow
		&& CardRow->TraitTags.HasTag(BG::Card::Trait::Attack.GetTag()))
	{
		const float KeepChance = ASC
			? ASC->GetNumericAttribute(UBGAttributeSet::GetAttackKeepCardUseChanceAttribute())
			: 0.f;

		if (KeepChance > 0.f)
		{
			UBGDiceComponent* Dice = UBGDiceComponent::Get(this);
			const int32 Roll = Dice ? Dice->RandomIndex(100) : INDEX_NONE;

			if (Roll != INDEX_NONE && static_cast<float>(Roll) < KeepChance
				&& MyPS->DecrementCardsUsedThisTurn(1) > 0)
			{
				bCardUseRefunded = true;

				//	GDD §10: 자동 판정은 원인·확률·결과를 로그에 남긴다.
				UE_LOG(LogBGCard, Log,
					TEXT("[무투가] 발동 — 눈 %d / 확률 %.0f%% → 카드 사용한도 유지"),
					Roll, KeepChance);

				BGDebug::Screen(TEXT("[무투가] 공격 카드의 사용한도를 소모하지 않았습니다."),
					FColor(120, 220, 160), /*Time=*/5.f);
			}
		}
	}

	if (MyPS)
	{
		if (const ABGGameState* GS = GetBGGameState())
		{
			UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 카드 사용 %d / %d"),
				GetMySeatIndex(), MyPS->GetCardsUsedThisTurn(), GS->GetCardUseLimitForSeat(GetMySeatIndex()));
		}
	}

}

// ======================================================================================
//  턴
// ======================================================================================

int32 ABGPlayerController::GetMySeatIndex() const
{
	const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	return PS ? PS->GetSeatIndex() : INDEX_NONE;
}

//	역할·직업은 PlayerState 의 사본을 먼저 본다(본인에게만 복제된다). 아직 안 왔으면 폰에서 읽는다 —
//	폰의 RoleTag 도 COND_OwnerOnly 라 어느 쪽이든 남의 값은 볼 수 없다.
FGameplayTag ABGPlayerController::GetMyRoleTag() const
{
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		if (PS->GetCachedRole().IsValid())
		{
			return PS->GetCachedRole();
		}
	}

	const ABGCharacter* MyCharacter = Cast<ABGCharacter>(GetPawn());
	return MyCharacter ? MyCharacter->GetRoleTag() : FGameplayTag();
}

FGameplayTag ABGPlayerController::GetMyJobTag() const
{
	if (const ABGPlayerState* PS = GetPlayerState<ABGPlayerState>())
	{
		if (PS->GetCachedJob().IsValid())
		{
			return PS->GetCachedJob();
		}
	}

	const ABGCharacter* MyCharacter = Cast<ABGCharacter>(GetPawn());
	return MyCharacter ? MyCharacter->GetJobTag() : FGameplayTag();
}

bool ABGPlayerController::DidIWin() const
{
	const FGameplayTag MyRole = GetMyRoleTag();
	if (!MyRole.IsValid())
	{
		//	⚠️ 조용히 패배로 처리하면 "전원이 패배 화면" 이 되고 원인을 짚기 어렵다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Match] 내 역할을 알 수 없어 승패를 판정하지 못했다 — 패배로 표시된다."));
		return false;
	}

	const ABGGameState* GS = GetBGGameState();
	return GS && GS->IsRoleWinner(MyRole);
}

bool ABGPlayerController::IsMyTurnToPlay() const
{
	const ABGGameState* GS = GetBGGameState();
	return GS && GS->CanSeatPlayCards(GetMySeatIndex());
}

bool ABGPlayerController::IsMyTurnToDiscard() const
{
	const ABGGameState* GS = GetBGGameState();
	return GS && GS->CanSeatDiscard(GetMySeatIndex());
}

void ABGPlayerController::RequestEndTurn()
{
	ServerRequestEndTurn();
}

void ABGPlayerController::ServerRequestEndTurn_Implementation()
{
	if (ABGGameState* GS = GetBGGameState())
	{
		//	좌석은 서버가 가진 PlayerState 에서 읽는다 — 클라가 보낸 값을 믿지 않는다.
		GS->RequestEndTurn(GetMySeatIndex());
	}
}

void ABGPlayerController::ServerRequestSkipIntro_Implementation()
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->SkipIntro();
	}
}

void ABGPlayerController::RequestReturnToLobby()
{
	ServerRequestReturnToLobby();
}

void ABGPlayerController::ServerRequestReturnToLobby_Implementation()
{
	if (ABGGameState* GS = GetBGGameState())
	{
		//	시점 검증은 서버가 한다 — 클라가 언제 눌렀는지는 믿지 않는다.
		GS->RequestReturnToLobbyFromResult();
	}
}

void ABGPlayerController::RequestStartPvEMatch()
{
	ServerRequestStartPvEMatch();
}

void ABGPlayerController::ServerRequestStartPvEMatch_Implementation()
{
	//	PvP 클라가 실수로(혹은 악의적으로) 이걸 불러도 세션 로비를 건너뛰지 못하게 막는다.
	if (ABGGameMode::GetPlayMode(this) != EBGPlayMode::PvE)
	{
		return;
	}

	if (UBGMatchStartComponent* Match = UBGMatchStartComponent::Find(GetWorld()))
	{
		Match->StartMatch();
	}
}

void ABGPlayerController::RequestSetPresentationDoubleSpeed(bool bEnable)
{
	ServerRequestSetPresentationDoubleSpeed(bEnable);
}

bool ABGPlayerController::IsPresentationDoubleSpeed() const
{
	const ABGGameState* GS = GetBGGameState();
	return IsValid(GS) && GS->IsPresentationDoubleSpeed();
}

void ABGPlayerController::ServerRequestSetPresentationDoubleSpeed_Implementation(bool bEnable)
{
	if (ABGGameState* GS = GetBGGameState(); IsValid(GS))
	{
		GS->SetPresentationDoubleSpeed(bEnable);
	}
}

void ABGPlayerController::RequestSelectPvEReward(FName CardId)
{
	ServerRequestSelectPvEReward(CardId);
}

void ABGPlayerController::ServerRequestSelectPvEReward_Implementation(FName CardId)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->ResolvePvERewardSelection(CardId);
	}
}

bool ABGPlayerController::ServerRequestDiscardCard_Validate(int32 InstanceId)
{
	return InstanceId != INDEX_NONE;
}

void ABGPlayerController::ServerRequestDiscardCard_Implementation(int32 InstanceId)
{
	ABGGameState* GS = GetBGGameState();
	const int32 MySeat = GetMySeatIndex();

	//	Discard 페이즈 + 자기 턴에서만 허용. 아니면 손패를 마음대로 버릴 수 있게 된다.
	if (!GS || !GS->CanSeatDiscard(MySeat))
	{
		BGDebug::Screen(TEXT("[서버] 지금은 버리기 페이즈가 아닙니다."), FColor::Red, 5.f);
		return;
	}

	ConsumeCard(InstanceId);
	GS->NotifyCardDiscarded(MySeat);
}

void ABGPlayerController::ServerSendPing_Implementation(FGameplayTag PingTag, int32 TargetSeat)
{
	if (ensureMsgf(Ping, TEXT("[BGPing] PingComponent 가 없다")))
	{
		Ping->HandleServerSendPing(PingTag, TargetSeat);
	}
}

bool ABGPlayerController::ServerSendPing_Validate(FGameplayTag PingTag, int32 TargetSeat)
{
	return TargetSeat >= INDEX_NONE;
}

void ABGPlayerController::ServerSendChatMessage_Implementation(const FString& Message)
{
	ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	const FString SenderName = PS ? PS->GetPlayerName() : TEXT("?Who?");
	
	ABGGameState* GS = GetBGGameState();
	if (!GS)
	{
		return;
	}

	//	판 도중 죽은 사람은 관전자다 — 관전자끼리만 본다. 산 사람의 채팅은 전원(관전자 포함)이 본다.
	if (PS && PS->IsDead() && GS->IsMatchRunning())
	{
		for (APlayerState* Other : GS->PlayerArray)
		{
			const ABGPlayerState* OtherPS = Cast<ABGPlayerState>(Other);
			ABGPlayerController* OtherPC = OtherPS ? Cast<ABGPlayerController>(OtherPS->GetOwningController()) : nullptr;
			if (OtherPC && OtherPS->IsDead())
			{
				OtherPC->ClientReceiveSpectatorChat(SenderName, Message);
			}
		}
		return;
	}

	GS->Multicast_ChatMessage(SenderName, Message);
}

void ABGPlayerController::ClientReceiveSpectatorChat_Implementation(const FString& SenderName, const FString& Message)
{
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->OnChatMessageReceived.Broadcast(SenderName, Message, /*bSpectator=*/ true);
	}
}

bool ABGPlayerController::ServerSendChatMessage_Validate(const FString& Message)
{
	return !Message.IsEmpty() && Message.Len() <= 256;
}

void ABGPlayerController::ConsumeCard(int32 InstanceId)
{
	ABGPlayerState* PS = GetPlayerState<ABGPlayerState>();
	if (!PS)
	{
		return;
	}

	// 손패 변경은 반드시 이 함수를 거친다 — HandCount 동기화와 OnHandChanged 통지가 여기 있다.
	FBGCardInstance Removed;
	if (!PS->RemoveCardFromHand(InstanceId, Removed))
	{
		// 발동 직전에 손패에서 찾아 검증했으므로 여기 걸리면 다른 경로가 손패를 건드린 것이다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[BG] ConsumeCard: Instance=%d 를 손패에서 찾지 못했습니다 (이미 제거됨?)."), InstanceId);
		return;
	}

	//	1회성 카드(암살자)는 버린 패로 가지 않고 사라진다. 덱에서 나온 카드가 아니므로
	//	버린 패에 넣으면 셔플을 거쳐 덱에 섞이고 **덱 총량이 판마다 늘어난다.**
	if (Removed.bEphemeral)
	{
		BGDebug::Screen(
			FString::Printf(TEXT("[서버] 1회성 카드 소멸  %s #%d  (남은 손패 %d 장)"),
				*Removed.CardId.ToString(), Removed.InstanceId, PS->GetHandCount()),
			FColor(200, 170, 120), /*Time=*/5.f);
		return;
	}

	// 버린 패로. DiscardTop 갱신과 DeckCount/DiscardCount 재계산이 함께 처리된다.
	if (ABGGameState* GS = GetBGGameState())
	{
		GS->PushToDiscard(Removed);
	}

	BGDebug::Screen(
		FString::Printf(TEXT("[서버] 카드 소비  %s #%d → 버린 패  (남은 손패 %d 장)"),
			*Removed.CardId.ToString(), Removed.InstanceId, PS->GetHandCount()),
		FColor(150, 200, 150), /*Time=*/5.f);
}

AActor* ABGPlayerController::ResolveTargetActor(int32 TargetSeat) const
{
	if (TargetSeat == INDEX_NONE)
	{
		return nullptr;
	}

	const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GS)
	{
		return nullptr;
	}

	for (APlayerState* PS : GS->PlayerArray)
	{
		const ABGPlayerState* BPS = Cast<ABGPlayerState>(PS);
		if (BPS && BPS->GetSeatIndex() == TargetSeat)
		{
			//	역참조가 비면 두고 간 폰을 대상으로 준다.
			APawn* Live = PS->GetPawn();
			return Live ? Live : BPS->GetAbandonedPawn();
		}
	}
	return nullptr;
}

void ABGPlayerController::HideGameplayWidgetsForEnding()
{
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, UUserWidget::StaticClass(), /*TopLevelOnly=*/true);

	for (UUserWidget* Widget : Widgets)
	{
		//	⚠️ 엔딩 위젯까지 숨기면 단계가 넘어갈 때마다 방금 띄운 위젯이 사라진다.
		if (!Widget || Widget->GetOwningPlayer() != this
			|| Widget == VictoryWidgetInstance || Widget == MatchResultWidgetInstance)
		{
			continue;
		}

		//	처음 숨길 때의 가시성만 기억한다 — 두 번째 호출에서 Collapsed 를 원래 값으로 덮으면 되살릴 수 없다.
		if (!EndingHiddenWidgets.Contains(Widget))
		{
			EndingHiddenWidgets.Add(Widget, Widget->GetVisibility());
		}
		Widget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
