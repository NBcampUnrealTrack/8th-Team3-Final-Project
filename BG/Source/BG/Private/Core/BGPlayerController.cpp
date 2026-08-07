#include "Core/BGPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"   // FGameplayEventData

#include "Debug/BGCardLog.h"
#include "Card/BGCardType.h"                // FBGCardInstance
#include "Debug/BGDebug.h"
#include "UI/View/BGCardViewLibrary.h"
#include "Card/BGDataSubsystem.h"           // GetCardRow
#include "Card/BGGameDataTypes.h"           // FBGCardRow
#include "Card/BGDiceComponent.h"           // 4단계 판정
#include "GAS/BGAttributeSet.h"             // GetOutcomeWeightAttributes
#include "BGGameplayTags.h"           // BG::Status::*
#include "Core/BGGameState.h"               // PushToDiscard
#include "Core/BGPlayerState.h"
#include "UI/View/BGSeatViewLibrary.h"
#include "GameFramework/GameStateBase.h"      // PlayerArray
#include "UI/Widget/BGHandWidget.h"
#include "UI/Widget/BGSeatBoardWidget.h"
#include "UI/Widget/BGTurnPanelWidget.h"
#include "UI/Widget/BGResolutionWidget.h"
#include "Core/BGCinematicCamera.h"          // 연출 카메라 (로컬 스폰)
#include "Core/BGCampfire.h"                 // 선회 중심 = 판의 중심
#include "Core/BGCharacter.h"                // 연출 주인공 캐스팅
#include "Camera/CameraShakeBase.h"          // 판정 셰이크
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

//	생성자는 없다 — GA 는 AbilityByCardId(BP 디폴트에서 채움)로만 조회한다.
//	하드코딩 폴백을 두지 않는 이유: 매핑이 비었을 때 엉뚱한 GA 로 조용히 새는 것보다
//	"GA 매핑 없음!" 을 화면에 띄우고 멈추는 편이 낫다(HandlePlayCard 참고).

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
	}

	HandWidget = InHandWidget;

	if (HandWidget)
	{
		HandWidget->OnCardPlayRequested.AddDynamic(this, &ABGPlayerController::HandleCardPlayRequested);
	}

	TryBindHandDelegate();
	TryBindAttributeDelegates();
	TryBindResolutionDelegate();
	TryBindCameraShotDelegate();
	TryBindTurnStateDelegate();
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
	const bool bSelectingTarget = SeatBoardWidget && SeatBoardWidget->IsSelectingTarget();

	//	Play 페이즈에서는 사용 횟수가 남아 있어야 조작할 수 있다(GDD §5.2).
	//	Discard 페이즈는 버리기라 한도와 무관하다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	const int32 MySeat = GetMySeatIndex();
	const bool bCanPlay    = IsMyTurnToPlay() && GS && GS->HasCardUsesLeft(MySeat);
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
	if (SeatBoardWidget && SeatBoardWidget->IsSelectingTarget())
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
}

void ABGPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라: PlayerState 가 이제야 도착했다. 여기서 구독하지 않으면 첫 손패를 놓친다.
	TryBindHandDelegate();
	TryBindAttributeDelegates();
	TryBindResolutionDelegate();
	TryBindCameraShotDelegate();
	TryBindTurnStateDelegate();
	RefreshHandWidget();
}

void ABGPlayerController::Destroyed()
{
	//	판이 도는 중이면 폰을 떼어 내 월드에 남긴다. Super 가 폰을 파괴하기 전 마지막 기회다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
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
	ResolutionWidget = InResolutionWidget;

	if (ResolutionWidget)
	{
		//	등록 직후에는 보여줄 판정이 없다. 켜진 채로 시작하면 빈 패널이 화면에 남는다.
		ResolutionWidget->HideResolution();
	}

	TryBindResolutionDelegate();
}

void ABGPlayerController::TryBindResolutionDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundGameState.Get() == GS)
	{
		return;   // GameState 가 아직 복제되지 않았거나 이미 구독 중
	}

	if (ABGGameState* Previous = BoundGameState.Get())
	{
		Previous->OnCardResolved.RemoveAll(this);
	}

	GS->OnCardResolved.AddUObject(this, &ABGPlayerController::HandleCardResolved);
	BoundGameState = GS;

	UE_LOG(LogBGCard, Verbose, TEXT("[PC] 판정 결과 통지 구독"));
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

void ABGPlayerController::HandleCardResolved(const FBGCardResolution& Resolution)
{
	//	Row 는 두 군데서 쓰인다 — 판정 위젯의 카드 이름, 그리고 손패 연출의 스텝 목록.
	//	(카드 이름을 DT 에서 가져오는 이유: 태그 문자열을 그대로 띄우면 플레이어가 읽을 수 없다.)
	const UBGDataSubsystem* Data = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UBGDataSubsystem>()
		: nullptr;
	const FBGCardRow* Row = Data ? Data->GetCardRow(Resolution.CardId) : nullptr;

	const EBGResolutionTier Tier = ToResolutionTier(Resolution.Outcome);

	//	── 손패 연출 통지 ──
	//	내 카드의 판정이면 중앙에 잡혀 있는 카드를 놓아 준다. 이것이 "효과가 발동했다" 의
	//	정식 신호다 — 손패에서 사라지는 것을 신호로 쓰면 등급을 알 수 없다.
	//
	//	⚠️ 아래 ResolutionWidget 조기 반환보다 먼저 와야 한다. 판정 표시 위젯을 아직
	//	   붙이지 않은 HUD 에서도 카드는 중앙에서 풀려야 하기 때문이다.
	//	맥락은 좌석과 무관하게 먼저 만든다. 손패 연출은 내 카드일 때만 쓰지만
	//	카메라 셰이크는 남의 카드에도 필요하기 때문이다.
	FBGCardPlayContext PlayContext;
	PlayContext.Tier      = Tier;
	PlayContext.Roll      = Resolution.Roll;
	PlayContext.bForced   = Resolution.bForced;
	PlayContext.SeatIndex = Resolution.SeatIndex;

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

	//	⚠️ 위 좌석 분기 **밖**이다. 안에 넣으면 내가 낸 카드에만 화면이 흔들려서,
	//	   같은 판을 보고 있는 다른 플레이어들만 조용한 이상한 그림이 된다.
	PlayResolveShake(PlayContext);

	if (!ResolutionWidget)
	{
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

	//	좌석 이름. 좌석 뷰를 거치지 않고 PlayerState 를 직접 찾는다 —
	//	좌석 보드가 없어도 판정 표시는 동작해야 한다.
	if (const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		if (const ABGPlayerState* SeatPS = GS->GetPlayerStateBySeat(Resolution.SeatIndex))
		{
			View.SeatName = FText::FromString(SeatPS->GetPlayerName());
		}
	}

	//	서버가 실제로 쓴 비율을 그대로 %로 바꾼다. 여기서 다시 보정을 계산하면
	//	남의 어트리뷰트를 읽어야 하는데 그건 복제되지 않아 남의 판정만 확률이 틀리게 나온다.
	UBGDiceComponent::GetOutcomeChances(Resolution.Weights,
		View.Chance.CriticalFailure, View.Chance.Failure,
		View.Chance.Success, View.Chance.CriticalSuccess);

	ResolutionWidget->ShowResolution(View);
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
	if (!ResolveShakeClass || !PlayerCameraManager || !IsLocalController())
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
	if (CinematicCamera || !IsLocalController())
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

void ABGPlayerController::TryBindCameraShotDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundCameraGameState.Get() == GS)
	{
		return;
	}

	if (ABGGameState* Previous = BoundCameraGameState.Get())
	{
		Previous->OnCameraShotChanged.RemoveAll(this);
	}

	GS->OnCameraShotChanged.AddUObject(this, &ABGPlayerController::HandleCameraShotChanged);
	BoundCameraGameState = GS;

	//	구독보다 샷이 먼저 시작됐을 수 있다(늦게 접속). 현재 상태를 즉시 한 번 적용한다 —
	//	이게 "이벤트가 아니라 상태" 로 둔 것의 값어치다.
	HandleCameraShotChanged(GS->GetCameraShot());
}

void ABGPlayerController::HandleCameraShotChanged(const FBGCameraShot& Shot)
{
	if (!IsLocalController())
	{
		return;   // 화면이 없는 컨트롤러에는 뷰타깃 개념이 없다.
	}

	//	연출 속도 설정은 GameState 가 들고 있다 — 세 구간을 한곳에서 조율하기 위해서다.
	//	EditDefaultsOnly(CDO) 값이라 클라에서도 복제 없이 그대로 읽힌다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;

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
	}

	RefreshSeatBoard();
}

void ABGPlayerController::BeginPlay()
{
	Super::BeginPlay();

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
}

void ABGPlayerController::TryBindTurnStateDelegate()
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || BoundTurnGameState.Get() == GS)
	{
		return;
	}

	GS->OnTurnStateChanged.AddUniqueDynamic(this, &ABGPlayerController::HandleTurnStateChanged);
	BoundTurnGameState = GS;

	//	구독 전에 이미 판이 시작됐을 수 있다 — 지금 상태로 한 번 맞춘다.
	HandleTurnStateChanged();

	UE_LOG(LogBGCard, Verbose, TEXT("[PC] 턴 상태 통지 구독"));
}

void ABGPlayerController::HandleTurnStateChanged()
{
	AbortActionIfTurnLost();

	RefreshTurnPanel();
	RefreshSeatBoard();
	UpdateHandInteractionLock();
	RefreshHandWidget();   //	카드의 사용 가능 표시가 페이즈에 걸려 있다.
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

	if (SeatBoardWidget && SeatBoardWidget->IsSelectingTarget())
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
	if (SeatBoardWidget)
	{
		SeatBoardWidget->SetSeats(UBGSeatViewLibrary::MakeSeatViews(this));
	}

	//	좌석 보드와 같은 타이머를 쓴다 — 갱신 주기가 갈리면 "턴은 넘어갔는데 버튼은 그대로" 가 된다.
	RefreshTurnPanel();

	//	턴/페이즈 잠금도 여기서 갱신한다. 페이즈 전환(Draw→Play)은 손패 내용을 바꾸지 않아
	//	OnHandChanged 가 오지 않는다 — 폴링하지 않으면 자기 차례가 되어도 손패가 잠긴 채 굳는다.
	UpdateHandInteractionLock();

	//	늦게 도착하는 것들에 대한 구독 재시도. 둘 다 이미 걸려 있으면 즉시 빠져나온다.
	//	  · GameState — 클라에서는 PlayerState 보다 늦게 올 수 있다. 놓치면 판정이 영영 안 뜬다.
	//	  · 폰의 ASC  — 손패보다 늦게 스폰되면 보정 구독이 비어, 장비를 껴도 확률이 안 바뀐다.
	TryBindResolutionDelegate();
	TryBindCameraShotDelegate();
	TryBindAttributeDelegates();
	TryBindTurnStateDelegate();
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

	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
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
			? NSLOCTEXT("BG", "TurnMineNoUses", "카드 사용 한도를 다 썼습니다 — 턴을 종료하세요")
			: NSLOCTEXT("BG", "TurnMine", "내 차례입니다");
	}
	else if (View.bMatchRunning)
	{
		View.StatusText = FText::FromString(
			FString::Printf(TEXT("좌석 %d 진행 중"), View.CurrentSeat));
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
	if (!SeatBoardWidget)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[PC] 좌석 보드가 없어 대상 선택을 시작할 수 없습니다."));
		return;
	}

	FBGTargetRequest Request;
	Request.ContextId        = ContextId;
	Request.Prompt           = Prompt;
	Request.SelectableSeats  = SelectableSeats;
	Request.bAllowCancel     = true;

	SeatBoardWidget->BeginTargetSelection(Request);
}

void ABGPlayerController::BeginTargetSelectionForCard(int32 CardInstanceId)
{
	//	사거리 판정은 아직 없다 — 살아 있는 다른 좌석 전부를 후보로 연다(STEP 7/8 에서 좁힌다).
	BeginTargetSelection(
		CardInstanceId,
		NSLOCTEXT("BG", "SelectTargetSeat", "대상을 선택하세요"),
		UBGSeatViewLibrary::GetSelectableSeats(this, /*bExcludeSelf=*/true));
}

void ABGPlayerController::CancelTargetSelection()
{
	if (SeatBoardWidget)
	{
		SeatBoardWidget->CancelTargetSelection();
	}
}

void ABGPlayerController::HandleSeatSelected(int32 SeatIndex, int32 ContextId)
{
	UE_LOG(LogBGCard, Log, TEXT("[PC] 대상 좌석 확정 — Seat=%d Context(카드 InstanceId)=%d"),
		SeatIndex, ContextId);

	UpdateHandInteractionLock();

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

void ABGPlayerController::HandlePlayCard(int32 InstanceId, int32 TargetSeat)
{
	UE_LOG(LogTemp, Log, TEXT("[BG] ServerRequestPlayCard 수신 — PC=%s Instance=%d TargetSeat=%d"),
		*GetName(), InstanceId, TargetSeat);

	// ── 턴/페이즈 검증 ──
	// 클라의 UI 표시는 힌트일 뿐이므로 서버가 반드시 다시 본다.
	// TODO: 사거리(WeaponRange vs 거리)·턴당 BANG 한도(BangLimit)는 아직 없다.
	{
		const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
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
			return;
		}

		//	턴당 카드 사용 한도 (GDD §5.2). 버리기는 이 경로를 타지 않으므로 영향받지 않는다.
		if (GBGIgnoreTurnOrder == 0 && GS && !GS->HasCardUsesLeft(MySeat))
		{
			const ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>();
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] 이번 턴 카드 사용 한도를 다 썼습니다 (%d / %d). 카드 #%d 거부."),
					MyPS ? MyPS->GetCardsUsedThisTurn() : 0,
					GS->GetCardUseLimitForSeat(MySeat),
					InstanceId),
				FColor::Orange, /*Time=*/5.f);
			return;
		}
	}

	// 시전자의 ASC 를 얻는다 (현재 ASC 는 캐릭터 소유).
	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetPawn());
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[BG] HandlePlayCard: 시전자 ASC 없음 (Pawn=%s)."),
			GetPawn() ? *GetPawn()->GetName() : TEXT("null"));
		return;
	}

	// ── InstanceId → 손패에서 카드 조회 → 카드 종류 태그 + 메커니즘 태그 ──
	// TODO(2단계): 손패에 없으면 거부는 이미 하고 있고, 실제 사용 시
	//              RemoveCardFromHand 로 그 인스턴스를 빼는 처리가 남았다.
	FGameplayTag CardId;    // 맵 조회 키 (Card.Id.*)
	FGameplayTag EventTag;  // 페이로드/로그용 메커니즘 태그 (Ability.*)
	const FBGCardRow* CardRow = nullptr;   // 판정 데이터 (OutcomeWeights / OutcomeMagnitudes)
	FName CardCardId = NAME_None;            // DT Row 이름 (장비 중복 검사용)
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
		return;
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
		return;
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
					return;
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
			return;
		}

		if (EquipPS->HasEquippedCard(CardCardId))
		{
			BGDebug::Screen(
				FString::Printf(TEXT("[서버] %s 는 이미 장착 중입니다 — 중복 장착 불가."),
					*CardRow->DisplayName.ToString()),
				FColor::Orange, /*Time=*/5.f);
			return;
		}

		FBGCardInstance Moved;
		if (!EquipPS->RemoveCardFromHand(InstanceId, Moved))
		{
			return;
		}

		if (!EquipPS->EquipCard(Moved, CardRow->EquipEffect))
		{
			//	장착 실패 — 카드를 손패로 되돌린다(카드만 사라지면 안 된다).
			EquipPS->AddCardToHand(Moved);
			return;
		}

		//	장비 설치도 카드 사용 횟수를 소비한다.
		//	GDD §5.3 이 미결정으로 둔 항목 — "소비한다" 로 우선 확정했다.
		//	바꾸려면 이 한 줄만 지우면 된다.
		EquipPS->IncrementCardsUsedThisTurn();

		BGDebug::Screen(
			FString::Printf(TEXT("장착  %s → %s"),
				*CardRow->DisplayName.ToString(),
				GetPawn() ? *GetPawn()->GetName() : TEXT("?")),
			FColor(120, 200, 255), /*Time=*/5.f);
		return;
	}

	//	판정 결과를 아래 "대비" 처리에서 쓴다(블록 밖에서 보이도록 선언).
	EBGDiceOutcome ResolvedOutcome = EBGDiceOutcome::Success;
	bool bResolved = false;

	// ── 4단계 판정 (GDD §4.2 / §9.1) ──
	//	판정은 서버가 여기서 한 번만 한다. GA 는 "결과 실행" 만 담당한다 —
	//	GA 마다 각자 굴리면 중복 판정과 "같은 판정을 두 번 적용" 위험이 생긴다(GDD §9.2).
	//	결과는 Resolution.* 태그로, 등급별 수치는 EventMagnitude 로 실어 보낸다.
	{
		UBGDiceComponent* Dice = UBGDiceComponent::Get(this);
		const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;

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
			const FBGOutcomeWeights Weights = Dice->ApplyModifiers(CardRow->OutcomeWeights, ASC);

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

			//	[비활성] EventMagnitude 는 전용 GA(GA_Bang/GA_Heal/GA_StealOrDiscard)의 폴백 경로다.
			//	  그 GA 들이 전부 비활성이라(GDD §13.1) 지금은 아무도 읽지 않는다.
			//	  실어 보내는 것 자체는 무해하고, 전용 GA 를 되살릴 때 이 줄이 있어야 하므로 남긴다.
			//	  주의: 다만 로그에는 찍지 않는다 — OutcomeMagnitudes 는 비활성 필드라 실제 효과와
			// 어긋난다(뱅 성공이 "수치 0" 으로, 함정이 "수치 1" 로 보였다).
			// 실제 효과는 바로 다음 줄에서 GA 가 요약을 출력한다.
			Payload.EventMagnitude = static_cast<float>(CardRow->OutcomeMagnitudes.ForOutcome(Outcome));

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
			if (ABGGameState* MutableGS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
			{
				FBGCardResolution Resolution;
				Resolution.CardId    = CardId;
				Resolution.Outcome   = Outcome;
				Resolution.Roll      = Roll;
				Resolution.bForced   = bForcedOutcome;
				Resolution.SeatIndex = GetPlayerState<ABGPlayerState>()
					? GetPlayerState<ABGPlayerState>()->GetSeatIndex()
					: INDEX_NONE;
				Resolution.Weights   = Weights;

				MutableGS->Multicast_CardResolved(Resolution);
			}
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
	ABGPlayerState* MyPS = GetPlayerState<ABGPlayerState>();
	if (MyPS)
	{
		MyPS->IncrementCardsUsedThisTurn();
	}

	const FGameplayAbilitySpecHandle ActivatedHandle = ASC->GiveAbilityAndActivateOnce(Spec, &Payload);

	// GiveAbilityAndActivateOnce 는 발동에 실패하면 ClearAbility 후 무효 핸들을 돌려준다
	// (AbilitySystemComponent_Abilities.cpp: InternalTryActivateAbility 실패 분기).
	// 즉 핸들 유효성이 곧 "발동 성공" 이다. 실패한 카드는 소비하지 않는다 —
	// GA 매핑 오류나 ActivationBlockedTags(State.Dead 등)로 카드가 조용히 사라지면 추적이 어렵다.
	if (!ActivatedHandle.IsValid())
	{
		//	발동 실패 — 미리 센 사용 횟수를 되돌린다.
		if (MyPS)
		{
			MyPS->DecrementCardsUsedThisTurn();
		}

		BGDebug::Screen(
			FString::Printf(TEXT("[서버] %s 발동 실패 — 카드 #%d 는 손패에 그대로 둡니다."),
				*AbilityClass->GetName(), InstanceId),
			FColor::Red, /*Time=*/8.f);
		return;
	}

	// 뱅 규칙: 낸 카드는 결과와 무관하게 버린 패로 간다.
	// (뱅!이 빗나가도 그 뱅! 카드는 돌아오지 않는다)
	ConsumeCard(InstanceId);

	//	사용 횟수는 발동 전에 이미 셌다(위 참조). 판정 실패도 소비된다 (GDD §5.3) —
	//	실패 여부는 GA 안에서 갈리므로 여기서는 아무 조건도 보지 않는다.
	//	── 대비(Status.NextCard.KeepCardUse) ──
	//	실패·대실패했다면 이 카드의 사용 횟수를 되돌린다. 상태는 결과와 무관하게 소모된다
	//	("다음에 사용하는 카드" 를 이미 썼으므로).
	if (MyPS && bResolved && MyPS->HasPendingStatus(BG::Status::NextCard::KeepCardUse.GetTag()))
	{
		int32 Unused = 0;
		MyPS->ConsumePendingStatus(BG::Status::NextCard::KeepCardUse.GetTag(), Unused);

		const bool bFailed = (ResolvedOutcome == EBGDiceOutcome::Failure
			|| ResolvedOutcome == EBGDiceOutcome::CriticalFailure);

		if (bFailed && MyPS->DecrementCardsUsedThisTurn(1) > 0)
		{
			BGDebug::Screen(TEXT("[대비] 실패했지만 카드 사용한도를 잃지 않았습니다."),
				FColor(120, 220, 160), /*Time=*/5.f);
		}
	}

	if (MyPS)
	{
		if (const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
		{
			UE_LOG(LogBGCard, Log, TEXT("[Turn] 좌석 %d 카드 사용 %d / %d"),
				GetMySeatIndex(), MyPS->GetCardsUsedThisTurn(), GS->GetCardUseLimitForSeat(GetMySeatIndex()));
		}
	}

	// ── 연출 (아트 기획서 "전투 연출") ──────────────────────────────
	//	맨 마지막에 시작한다. 판정·효과 적용·카드 소비가 모두 끝난 뒤라야
	//	연출이 규칙에 관여하지 않는다는 원칙(Card.Trait.Cinematic 주석)이 지켜진다.
	//	연출이 켜지든 말든 여기까지의 결과는 완전히 동일하다.
	if (CardRow->TraitTags.HasTag(BG::Card::Trait::Cinematic.GetTag()))
	{
		if (ABGGameState* MutableGS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
		{
			//	Primary 주인공은 카드를 쓴 사람, Secondary 는 대상이다.
			//	대상이 없는 카드(회복 등)는 GameState 가 사용자를 계속 비춘다.
			MutableGS->BeginCardCinematic(
				Cast<ABGCharacter>(GetPawn()),
				Cast<ABGCharacter>(ResolveTargetActor(TargetSeat)));
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

bool ABGPlayerController::IsMyTurnToPlay() const
{
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	return GS && GS->CanSeatPlayCards(GetMySeatIndex());
}

bool ABGPlayerController::IsMyTurnToDiscard() const
{
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	return GS && GS->CanSeatDiscard(GetMySeatIndex());
}

void ABGPlayerController::RequestEndTurn()
{
	ServerRequestEndTurn();
}

void ABGPlayerController::ServerRequestEndTurn_Implementation()
{
	if (ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
	{
		//	좌석은 서버가 가진 PlayerState 에서 읽는다 — 클라가 보낸 값을 믿지 않는다.
		GS->RequestEndTurn(GetMySeatIndex());
	}
}

bool ABGPlayerController::ServerRequestDiscardCard_Validate(int32 InstanceId)
{
	return InstanceId != INDEX_NONE;
}

void ABGPlayerController::ServerRequestDiscardCard_Implementation(int32 InstanceId)
{
	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
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

	// 버린 패로. DiscardTop 갱신과 DeckCount/DiscardCount 재계산이 함께 처리된다.
	if (ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr)
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
			return PS->GetPawn();
		}
	}
	return nullptr;
}
