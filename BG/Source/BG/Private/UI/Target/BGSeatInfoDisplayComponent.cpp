#include "UI/Target/BGSeatInfoDisplayComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"
#include "GAS/BGAttributeSet.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "UI/Presentation/BGPresentationDirector.h"
#include "UI/View/BGSeatViewLibrary.h"
#include "UI/Widget/BGTurnPanelWidget.h"

UBGSeatInfoDisplayComponent::UBGSeatInfoDisplayComponent()
{
	//	커서 위치는 매 프레임 바뀐다 — 호버 판정만은 폴링을 피할 수 없다.
	PrimaryComponentTick.bCanEverTick = true;

	//	WBP 가 있으면 그것을 쓰고, 없으면 C++ 클래스가 최소 배치를 스스로 만든다.
	//	다른 WBP 를 쓰려면 BP_BGPlayerController 에서 이 값만 바꾸면 된다.
	InfoWidgetClass = UBGSeatInfoWidget::StaticClass();

	static ConstructorHelpers::FClassFinder<UBGSeatInfoWidget>
		InfoBP(TEXT("/Game/UI/WBP_BGSeatPlate"));
	if (InfoBP.Succeeded()) { InfoWidgetClass = InfoBP.Class; }

	static ConstructorHelpers::FClassFinder<UBGSeatDetailWidget>
		DetailBP(TEXT("/Game/UI/WBP_BGDetailPanel"));
	if (DetailBP.Succeeded()) { DetailWidgetClass = DetailBP.Class; }
}

ABGPlayerController* UBGSeatInfoDisplayComponent::GetOwningController() const
{
	return Cast<ABGPlayerController>(GetOwner());
}

void UBGSeatInfoDisplayComponent::BeginPlay()
{
	Super::BeginPlay();

	//	보는 사람 화면에만 있는 것이다. 서버가 남의 컨트롤러에 대해 돌릴 이유가 없다.
	const ABGPlayerController* PC = GetOwningController();
	if (!PC || !PC->IsLocalController())
	{
		SetComponentTickEnabled(false);
		return;
	}

	//	좌석이 배정되는 시점을 알려주는 통지가 없다. 짧게 확인하다 배정되면 스스로 켠다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(SeatWaitTimer, this,
			&UBGSeatInfoDisplayComponent::PollSeatAssigned, 0.5f, /*bLoop=*/true);
	}
}

//	이름표는 로비에서도 띄운다 — 누가 어느 자리인지 시작 전에 알아야 한다.
//	체력은 직업 배정 전에 값이 없고, 그때는 이름표가 알아서 그 줄만 감춘다.
void UBGSeatInfoDisplayComponent::PollSeatAssigned()
{
	const ABGPlayerController* PC = GetOwningController();
	const ABGPlayerState* PS = PC ? PC->GetPlayerState<ABGPlayerState>() : nullptr;

	SetDisplayEnabled(PS && PS->GetSeatIndex() != INDEX_NONE);
}

void UBGSeatInfoDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SeatWaitTimer);
	}

	UnbindDelegates();
	DestroyAllWidgets();

	Super::EndPlay(EndPlayReason);
}

void UBGSeatInfoDisplayComponent::SetDisplayEnabled(bool bEnabled)
{
	if (bDisplayEnabled == bEnabled)
	{
		return;
	}

	bDisplayEnabled = bEnabled;

	const ABGPlayerController* PC = GetOwningController();
	const bool bLocal = PC && PC->IsLocalController();

	SetComponentTickEnabled(bEnabled && bLocal);

	if (!bEnabled)
	{
		UnbindDelegates();
		HideDetail();
		DestroyAllWidgets();

		HeldHealthBySeat.Reset();
		PlateOpacityBySeat.Reset();
		bPlatesHidden = false;
		bPresentationActive = false;
		bHitReached = false;
		bPrimaryStage = false;
		bIntroActive = false;
		IntroFocusSeat = INDEX_NONE;

		if (HoveredSeat != INDEX_NONE)
		{
			HoveredSeat = INDEX_NONE;
			OnHoveredSeatChanged.Broadcast(HoveredSeat);
		}
		return;
	}

	//	켜자마자 한 번 맞춰 둔다 — 통지를 기다리며 빈 화면을 보여주지 않기 위해서다.
	BindDelegates();
	TimeSinceStructureCheck = 0.f;
	SyncWidgets();
	RefreshWidgets();
}

void UBGSeatInfoDisplayComponent::BindDelegates()
{
	if (!PublicInfoHandle.IsValid())
	{
		PublicInfoHandle = ABGPlayerState::OnAnySeatPublicInfoChanged.AddUObject(
			this, &UBGSeatInfoDisplayComponent::HandleSeatPublicInfoChanged);
	}

	if (!DisconnectHandle.IsValid())
	{
		DisconnectHandle = ABGPlayerState::OnAnyDisconnectChanged.AddUObject(
			this, &UBGSeatInfoDisplayComponent::HandleDisconnectChanged);
	}

	ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (GS && BoundGameState.Get() != GS)
	{
		GS->OnTurnStateChanged.AddUniqueDynamic(this, &UBGSeatInfoDisplayComponent::HandleTurnStateChanged);
		CinematicStageHandle = GS->OnCinematicStageChanged.AddUObject(
			this, &UBGSeatInfoDisplayComponent::HandleCinematicStageChanged);
		IntroStageHandle = GS->OnIntroStageChanged.AddUObject(
			this, &UBGSeatInfoDisplayComponent::HandleIntroStageChanged);
		BoundGameState = GS;

		//	연출 도중에 켜졌을 수 있다 — 통지를 기다리지 않고 지금 단계에 맞춘다.
		HandleCinematicStageChanged(GS->GetCinematicStage(), GS->GetCinematicSubject());
		HandleIntroStageChanged(GS->GetIntroStage());
	}

	//	HUD 가 걷히는 신호. 같은 PlayerController 에 붙어 있다.
	UBGPresentationDirector* Director = GetOwner() ? GetOwner()->FindComponentByClass<UBGPresentationDirector>() : nullptr;
	if (Director && BoundDirector.Get() != Director)
	{
		Director->OnPresentationStarted.AddUniqueDynamic(this, &UBGSeatInfoDisplayComponent::HandlePresentationStarted);
		Director->OnPresentationFinished.AddUniqueDynamic(this, &UBGSeatInfoDisplayComponent::HandlePresentationFinished);
		BoundDirector = Director;

		if (Director->IsPresenting())
		{
			HandlePresentationStarted(Director->GetActiveRequest());
		}
	}
}

void UBGSeatInfoDisplayComponent::UnbindDelegates()
{
	if (PublicInfoHandle.IsValid())
	{
		ABGPlayerState::OnAnySeatPublicInfoChanged.Remove(PublicInfoHandle);
		PublicInfoHandle.Reset();
	}

	if (DisconnectHandle.IsValid())
	{
		ABGPlayerState::OnAnyDisconnectChanged.Remove(DisconnectHandle);
		DisconnectHandle.Reset();
	}

	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnTurnStateChanged.RemoveDynamic(this, &UBGSeatInfoDisplayComponent::HandleTurnStateChanged);
		GS->OnCinematicStageChanged.Remove(CinematicStageHandle);
		GS->OnIntroStageChanged.Remove(IntroStageHandle);
	}
	CinematicStageHandle.Reset();
	IntroStageHandle.Reset();
	BoundGameState = nullptr;

	if (UBGPresentationDirector* Director = BoundDirector.Get())
	{
		Director->OnPresentationStarted.RemoveDynamic(this, &UBGSeatInfoDisplayComponent::HandlePresentationStarted);
		Director->OnPresentationFinished.RemoveDynamic(this, &UBGSeatInfoDisplayComponent::HandlePresentationFinished);
	}
	BoundDirector = nullptr;

	//	⚠️ 체력 구독도 떼야 한다. 목록만 비우면 다시 켤 때 같은 ASC 에 두 번 걸린다.
	for (const TPair<int32, TWeakObjectPtr<UAbilitySystemComponent>>& Pair : BoundHealthASCs)
	{
		if (UAbilitySystemComponent* ASC = Pair.Value.Get())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetHealthAttribute()).RemoveAll(this);
			ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
		}
	}
	BoundHealthASCs.Reset();
}

void UBGSeatInfoDisplayComponent::HandleTurnStateChanged()
{
	//	"지금 차례" 표시와 생사가 바뀌었을 수 있다. 좌석이 늘거나 줄었을 수도 있다.
	SyncWidgets();
	RefreshWidgets();
}

void UBGSeatInfoDisplayComponent::HandleSeatPublicInfoChanged(int32 /*SeatIndex*/)
{
	//	한 좌석만 바뀌어도 전체를 다시 만든다. 7명치라 나눠 갱신할 이득이 없다.
	RefreshWidgets();
}

void UBGSeatInfoDisplayComponent::HandleDisconnectChanged(int32 /*SeatIndex*/, bool /*bDisconnected*/)
{
	RefreshWidgets();
}

double UBGSeatInfoDisplayComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

//	피해·회복은 연출보다 먼저 적용된다(규칙 → 판정 UI → 공격 → 피격 순). 그대로 그리면
//	주사위가 굴러가는 동안 결과가 하트로 드러난다. 그래서 변화는 일단 붙잡고,
//	피격 연출에 와서 푼다. 연출이 따라오지 않으면 유예 뒤에 곧장 푼다.
void UBGSeatInfoDisplayComponent::HandleHealthChanged(const FOnAttributeChangeData& Data, int32 SeatIndex)
{
	//	⚠️ 값이 같은 통지는 무시해야 한다. 서버에서는 GE 적용 뒤 PostGameplayEffectExecute 의 클램프가
	//	   한 번 더 통지할 수 있다 — 이걸 회복으로 치면 붙잡은 데미지가 풀린다.
	if (FMath::IsNearlyEqual(Data.NewValue, Data.OldValue))
	{
		return;
	}

	//	회복도 데미지와 똑같이 붙잡는다. 회복 역시 판정 UI 보다 먼저 적용돼서, 바로 그리면
	//	주사위가 굴러가는 동안 하트가 차 결과가 드러난다.

	const double Now = GetNow();

	if (HeldHealthBySeat.IsEmpty())
	{
		HealthHoldStartTime = Now;
	}
	HoldGraceAnchorTime = Now;

	//	이미 붙잡은 좌석이면 처음 값을 지킨다 — 연출 한 번에 여러 번 바뀌어도 연출 전 값을 보여줘야 한다.
	if (!HeldHealthBySeat.Contains(SeatIndex))
	{
		HeldHealthBySeat.Add(SeatIndex, FMath::RoundToInt(Data.OldValue));
	}

	//	피격 연출 도중에 늦게 온 변화 같은 경우는 여기서 바로 풀린다.
	if (!TickHealthHold())
	{
		RefreshWidgets();
	}
}

void UBGSeatInfoDisplayComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& /*Data*/)
{
	RefreshWidgets();
}

bool UBGSeatInfoDisplayComponent::TickHealthHold()
{
	if (HeldHealthBySeat.IsEmpty())
	{
		return false;
	}

	const double Now = GetNow();

	if (Now - HealthHoldStartTime >= MaxHealthHoldSeconds)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[SeatInfo] 체력 표시를 %.1f초 넘게 붙잡았습니다 — 연출이 끝나지 않은 채 풀어 줍니다."),
			MaxHealthHoldSeconds);
		ReleaseHeldHealth();
		return true;
	}

	const ABGGameState* GS = BoundGameState.Get();
	const EBGCinematicStage Stage = GS ? GS->GetCinematicStage() : EBGCinematicStage::None;

	//	판정 UI → 공격 연출로 넘어갈 때 두 신호가 한 프레임 어긋나 "아무것도 아닌" 틈이 보일 수 있다.
	//	유예를 마지막으로 연출 중이던 순간부터 재서 그 틈에 풀리지 않게 한다.
	//	HUD 가 걷혀 있는 동안(카드 연출이 돌고 아직 피격 전)도 같은 대기로 본다.
	const bool bResolving = (GS && GS->IsResolutionPresentationPlaying()) || (bPresentationActive && !bHitReached);
	if (bResolving || Stage == EBGCinematicStage::Primary)
	{
		HoldGraceAnchorTime = Now;
	}

	bool bRelease = false;
	switch (Stage)
	{
	case EBGCinematicStage::Primary:
		break;   //	공격 연출 중 — 아직 맞지 않았다.

	case EBGCinematicStage::Secondary:
		bRelease = Now - SecondaryStageStartTime >= HealthRevealDelay;
		break;

	case EBGCinematicStage::None:
		//	판정 UI 가 도는 동안은 뒤에 연출이 올 수 있다. 그것도 아니면 유예만큼 기다려 본다.
		bRelease = !bResolving && Now - HoldGraceAnchorTime >= HealthHoldGraceSeconds;
		break;

	default:
		bRelease = true;   //	사망 연출은 피격 뒤다 — 붙잡을 이유가 없다.
		break;
	}

	if (!bRelease)
	{
		return false;
	}

	//	내 하트는 HUD(턴 패널) 안에 있다. HUD 는 연출이 다 끝나야 돌아오므로, 남들과 같이 풀면
	//	숨어 있는 동안 깎여 버려 꺼지는 연출을 못 본다 — HUD 가 다 떠오른 뒤에 푼다.
	const int32 LocalSeat = GetLocalSeatIndex();
	const bool bHoldLocal = LocalSeat != INDEX_NONE && HeldHealthBySeat.Contains(LocalSeat)
		&& (bPresentationActive || Now - PresentationFinishedTime < LocalHealthRevealDelay);

	if (!bHoldLocal)
	{
		ReleaseHeldHealth();
		return true;
	}

	//	남은 것이 내 좌석뿐이면 바뀐 것이 없다 — 매 프레임 다시 그리지 않는다.
	if (HeldHealthBySeat.Num() == 1)
	{
		return false;
	}

	const int32 LocalHeld = HeldHealthBySeat[LocalSeat];
	HeldHealthBySeat.Reset();
	HeldHealthBySeat.Add(LocalSeat, LocalHeld);
	RefreshWidgets();
	return true;
}

int32 UBGSeatInfoDisplayComponent::GetLocalSeatIndex() const
{
	const ABGPlayerController* PC = GetOwningController();
	const ABGPlayerState* PS = PC ? PC->GetPlayerState<ABGPlayerState>() : nullptr;
	return PS ? PS->GetSeatIndex() : INDEX_NONE;
}

void UBGSeatInfoDisplayComponent::ReleaseHeldHealth()
{
	HeldHealthBySeat.Reset();
	RefreshWidgets();
}

//	HUD(WBP_BGHUD)가 카드 연출 시작에 맞춰 걷힌다 — 이름표도 같은 신호로 걷는다.
//	다시 띄우는 것은 HUD 보다 이르다: 피격 연출에서 맞는 순간을 보여줘야 하기 때문이다.
void UBGSeatInfoDisplayComponent::HandlePresentationStarted(const FBGPresentationRequest& Request)
{
	if (Request.Kind != EBGPresentationKind::CardPlay)
	{
		return;
	}

	bPresentationActive = true;
	bHitReached = false;
	UpdatePlateVisibility();
}

void UBGSeatInfoDisplayComponent::HandlePresentationFinished(const FBGPresentationRequest& /*Request*/)
{
	//	피격 단계가 없는 카드(비시네마틱)는 여기서 돌아온다 — HUD 와 같은 때다.
	bPresentationActive = false;
	PresentationFinishedTime = GetNow();
	UpdatePlateVisibility();
}

void UBGSeatInfoDisplayComponent::HandleCinematicStageChanged(EBGCinematicStage Stage, ABGCharacter* /*Subject*/)
{
	if (Stage == EBGCinematicStage::Secondary)
	{
		SecondaryStageStartTime = GetNow();
		bHitReached = true;
	}

	bPrimaryStage = Stage == EBGCinematicStage::Primary;
	UpdatePlateVisibility();
}

//	도입 연출은 "한 사람만 비추는" 구간이다 — 용사 공개는 용사만, 첫 턴 안내는 시작 좌석만.
//	카드 연출과 달리 좌석마다 답이 다르므로 투명도도 좌석별로 간다.
void UBGSeatInfoDisplayComponent::HandleIntroStageChanged(EBGIntroStage Stage)
{
	const ABGGameState* GS = BoundGameState.Get();

	bIntroActive = Stage != EBGIntroStage::None;

	IntroFocusSeat = INDEX_NONE;
	if (GS)
	{
		if (Stage == EBGIntroStage::HeroReveal)
		{
			IntroFocusSeat = GS->GetHeroSeat();
		}
		else if (Stage == EBGIntroStage::FirstTurn)
		{
			IntroFocusSeat = GS->GetFirstTurnSeat();
		}
	}

	UpdatePlateVisibility();
}

float UBGSeatInfoDisplayComponent::GetTargetPlateOpacity(int32 SeatIndex) const
{
	if (bPlatesHidden)
	{
		return 0.f;   //	카드 연출 — 전원 걷는다.
	}

	if (!bIntroActive)
	{
		return 1.f;
	}

	//	도입 연출 중에는 비추는 좌석 하나만 뜬다. 비출 좌석이 없는 단계면 전부 걷힌다.
	return SeatIndex == IntroFocusSeat ? 1.f : 0.f;
}

void UBGSeatInfoDisplayComponent::UpdatePlateVisibility()
{
	//	Primary 도 따로 본다 — 디렉터 신호가 빠져도 공격 연출 동안은 걷혀 있어야 한다.
	bPlatesHidden = (bPresentationActive && !bHitReached) || bPrimaryStage;

	//	목표가 바뀌면 상세도 따라가야 한다. 커서가 그대로 올라가 있으면 호버 통지가 다시 오지 않는다.
	if (HoveredSeat != INDEX_NONE && GetTargetPlateOpacity(HoveredSeat) <= 0.f)
	{
		HideDetail();
	}
	else if (HoveredSeat != INDEX_NONE && VisibleDetailSeat == INDEX_NONE)
	{
		ShowDetailFor(HoveredSeat);
	}
}

void UBGSeatInfoDisplayComponent::TickPlateFade(float DeltaTime)
{
	for (const TPair<int32, TObjectPtr<UWidgetComponent>>& Pair : WidgetsBySeat)
	{
		const int32 Seat = Pair.Key;
		const float Target = GetTargetPlateOpacity(Seat);

		float& Opacity = PlateOpacityBySeat.FindOrAdd(Seat, 1.f);
		if (Opacity == Target)
		{
			continue;
		}

		const float Seconds = Target < Opacity ? PlateFadeOutSeconds : PlateFadeInSeconds;
		Opacity = Seconds > 0.f
			? FMath::FInterpConstantTo(Opacity, Target, DeltaTime, 1.f / Seconds)
			: Target;

		ApplyPlateOpacity(Pair.Value, Opacity);
	}
}

void UBGSeatInfoDisplayComponent::ApplyPlateOpacity(UWidgetComponent* Widget, float Opacity) const
{
	if (!Widget)
	{
		return;
	}

	if (UUserWidget* UserWidget = Widget->GetUserWidgetObject())
	{
		UserWidget->SetRenderOpacity(Opacity);
	}

	//	다 사라졌으면 컴포넌트째 끈다. 투명한 채로 그리는 비용을 아낀다.
	Widget->SetVisibility(Opacity > 0.f);
}

void UBGSeatInfoDisplayComponent::BindHealthDelegate(ABGCharacter* Character, int32 SeatIndex)
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	if (!ASC)
	{
		return;   //	폰은 있는데 ASC 가 아직 없다. 다음 구조 확인에서 다시 본다.
	}

	const TWeakObjectPtr<UAbilitySystemComponent> Previous = BoundHealthASCs.FindRef(SeatIndex);
	if (Previous.Get() == ASC)
	{
		return;
	}

	//	같은 좌석에 폰이 바뀌었다(복귀·재스폰). 옛 ASC 에 남은 구독을 뗀다.
	if (UAbilitySystemComponent* OldASC = Previous.Get())
	{
		OldASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetHealthAttribute()).RemoveAll(this);
		OldASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetMaxHealthAttribute()).RemoveAll(this);
	}

	//	Health 와 MaxHealth 둘 다 본다. 최대치만 바뀌는 경우(직업 배정)도 표시가 달라진다.
	//	Health 에는 좌석을 실어 보낸다 — 변화 전 값을 좌석별로 붙잡아야 한다.
	ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UBGSeatInfoDisplayComponent::HandleHealthChanged, SeatIndex);
	ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UBGSeatInfoDisplayComponent::HandleMaxHealthChanged);

	BoundHealthASCs.Add(SeatIndex, ASC);
}

void UBGSeatInfoDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bDisplayEnabled)
	{
		return;
	}

	//	커서 위치에는 통지가 없다 — 호버만은 매 프레임 본다.
	UpdateHover();

	//	페이드와 체력 공개 시점도 시간 조건이라 통지로는 잡을 수 없다.
	TickPlateFade(DeltaTime);
	TickHealthHold();

	//	값 갱신은 델리게이트가 한다. 여기서는 늦게 도착한 폰만 주워 담는다.
	TimeSinceStructureCheck += DeltaTime;
	if (StructureCheckInterval > 0.f && TimeSinceStructureCheck < StructureCheckInterval)
	{
		return;
	}
	TimeSinceStructureCheck = 0.f;

	//	구독도 여기서 재시도한다. GameState 는 클라에서 늦게 오고,
	//	폰의 ASC 는 위젯보다 늦게 붙을 수 있다.
	BindDelegates();

	const int32 Before = WidgetsBySeat.Num();
	SyncWidgets();

	//	새로 붙은 좌석이 있을 때만 다시 그린다. 없으면 통지가 이미 최신으로 맞춰 뒀다.
	if (WidgetsBySeat.Num() != Before)
	{
		RefreshWidgets();
	}
}

ABGCharacter* UBGSeatInfoDisplayComponent::GetHoveredCharacter() const
{
	if (HoveredSeat == INDEX_NONE)
	{
		return nullptr;
	}

	const ABGPlayerState* PS = UBGSeatViewLibrary::FindPlayerStateBySeat(GetOwningController(), HoveredSeat);
	return PS ? Cast<ABGCharacter>(PS->GetPawn()) : nullptr;
}

UWidgetComponent* UBGSeatInfoDisplayComponent::EnsureWidgetFor(ABGCharacter* Character, int32 SeatIndex)
{
	if (!Character)
	{
		return nullptr;
	}

	//	구조 확인이 1초마다 도는 자리다. 매번 찍으면 로그가 도배되므로 한 번만 알린다.
	if (!InfoWidgetClass)
	{
		if (!bWarnedMissingInfoClass)
		{
			bWarnedMissingInfoClass = true;
			UE_LOG(LogBGCard, Warning,
				TEXT("[SeatInfo] InfoWidgetClass 가 비어 있습니다 — 머리 위 이름표가 뜨지 않습니다."));
		}
		return nullptr;
	}

	if (const TObjectPtr<UWidgetComponent>* Existing = WidgetsBySeat.Find(SeatIndex))
	{
		//	같은 좌석이라도 폰이 바뀔 수 있다(복귀·재스폰). 주인이 다르면 새로 만든다.
		if (*Existing && (*Existing)->GetOwner() == Character)
		{
			return *Existing;
		}

		if (*Existing)
		{
			(*Existing)->DestroyComponent();
		}
		WidgetsBySeat.Remove(SeatIndex);
	}

	const float CapsuleTop = Character->GetCapsuleComponent()
		? Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.f;

	//	둘 다 같은 월드 지점에 걸고 위아래는 픽셀로 나눈다. Pivot.Y=1 이면 아래 끝이 그 지점이다.
	const float AnchorZ = CapsuleTop + HeightAboveCapsule;

	//	이름표도 가로로 밀 수 있게 한다. Pivot.X 를 줄이면 오른쪽으로 간다.
	const float PlateShift = WidgetDrawSize.X > 0.f ? PlateScreenOffsetX / WidgetDrawSize.X : 0.f;

	UWidgetComponent* Widget = MakeWidgetOn(Character, InfoWidgetClass, WidgetDrawSize,
		AnchorZ, FVector2D(0.5f - PlateShift, 1.f));
	if (!Widget)
	{
		return nullptr;
	}
	WidgetsBySeat.Add(SeatIndex, Widget);

	//	연출 도중에 붙었으면 지금 있어야 할 투명도로 시작한다. 안 그러면 혼자 불쑥 뜬다.
	const float Opacity = GetTargetPlateOpacity(SeatIndex);
	PlateOpacityBySeat.Add(SeatIndex, Opacity);
	ApplyPlateOpacity(Widget, Opacity);

	//	상세는 이름표 위에 얹는다. 평소에는 숨어 있다가 커서를 올렸을 때만 켜진다.
	if (DetailWidgetClass && DetailDrawSize.Y > 0.f)
	{
		//	이름표 높이와 틈만큼 Pivot 을 더 밀어 올린다. 크기가 바뀌어도 간격이 따라온다.
		const float LiftRatio = (WidgetDrawSize.Y + DetailGap) / DetailDrawSize.Y;

		//	가로도 같은 원리다. Pivot.X 를 줄이면 위젯이 오른쪽으로 간다.
		const float ShiftRatio = DetailDrawSize.X > 0.f ? DetailScreenOffsetX / DetailDrawSize.X : 0.f;

		if (UWidgetComponent* Detail = MakeWidgetOn(Character, DetailWidgetClass, DetailDrawSize,
				AnchorZ, FVector2D(0.5f - ShiftRatio, 1.f + LiftRatio)))
		{
			Detail->SetVisibility(false);
			DetailsBySeat.Add(SeatIndex, Detail);
		}
	}

	return Widget;
}

UWidgetComponent* UBGSeatInfoDisplayComponent::MakeWidgetOn(
	ABGCharacter* Character, TSubclassOf<UUserWidget> WidgetClass, const FVector2D& DrawSize,
	float HeightZ, const FVector2D& Pivot)
{
	UWidgetComponent* Widget = NewObject<UWidgetComponent>(Character);
	if (!Widget)
	{
		return nullptr;
	}

	Widget->SetupAttachment(Character->GetRootComponent());
	Widget->SetWidgetSpace(bDrawInWorldSpace ? EWidgetSpace::World : EWidgetSpace::Screen);
	Widget->SetDrawSize(DrawSize);
	Widget->SetWidgetClass(WidgetClass);

	//	주의: 위젯이 커서 트레이스를 가로채면 정작 캐릭터를 못 고른다. 반드시 꺼야 한다.
	Widget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Widget->SetGenerateOverlapEvents(false);
	Widget->SetCullDistance(MaxDrawDistance);
	Widget->SetRelativeLocation(FVector(0.f, 0.f, HeightZ));

	//	화면 공간 위젯은 픽셀 크기가 고정이라 세로 간격도 픽셀로 줘야 한다.
	//	월드 Z 로 벌리면 멀어질수록 화면상 간격이 줄어 이름표와 겹친다.
	Widget->SetPivot(Pivot);

	Widget->RegisterComponent();
	Widget->InitWidget();   //	위젯 인스턴스를 지금 만들어 둔다 — 바로 값을 밀어넣기 위해서다.

	return Widget;
}

void UBGSeatInfoDisplayComponent::SyncWidgets()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const ABGPlayerController* PC = GetOwningController();
	const ABGPlayerState* LocalState = PC ? PC->GetPlayerState<ABGPlayerState>() : nullptr;

	TSet<int32> LiveSeats;

	//	PlayerState 목록이 아니라 월드의 캐릭터를 돈다.
	//	APlayerState::GetPawn() 은 복제되는 값이 아니라 폰이 도착해 되꽂아 주는 역참조라,
	//	클라에서 폰보다 PlayerState 가 먼저 오면 한동안 비어 있다 — 그동안 이름표가 안 뜬다.
	//	폰의 PlayerState 는 정식으로 복제되므로 이쪽에서 보면 그 구멍이 없다.
	for (TActorIterator<ABGCharacter> It(World); It; ++It)
	{
		ABGCharacter* Character = *It;

		if (!Character)
		{
			continue;
		}

		const ABGPlayerState* PS = Character->GetPlayerState<ABGPlayerState>();

		//	폰에서 PlayerState 를 못 읽으면 기억해 둔 좌석으로 되찾는다.
		if (!PS)
		{
			if (const int32* Remembered = SeatByCharacter.Find(Character))
			{
				PS = UBGSeatViewLibrary::FindPlayerStateBySeat(PC, *Remembered);
			}
		}

		if (!PS || PS->GetSeatIndex() == INDEX_NONE)
		{
			continue;
		}

		//	폰과 좌석의 짝을 기억해 둔다. 나중에 컨트롤러가 떨어져도 이 값으로 되찾는다.
		SeatByCharacter.Add(Character, PS->GetSeatIndex());

		//	자기 머리 위에는 띄우지 않는다. 자기 정보는 화면 좌하단 전용 패널이 맡는다.
		if (PS == LocalState)
		{
			//	그래도 체력은 듣는다 — 턴 패널의 내 하트도 피격 연출에 맞춰 깎여야 한다.
			BindHealthDelegate(Character, PS->GetSeatIndex());
			continue;
		}

		if (EnsureWidgetFor(Character, PS->GetSeatIndex()))
		{
			LiveSeats.Add(PS->GetSeatIndex());

			//	체력은 PlayerState 가 아니라 폰의 ASC 에 있다 — 좌석마다 따로 걸어야 한다.
			BindHealthDelegate(Character, PS->GetSeatIndex());
		}
	}

	//	사라진 좌석의 위젯을 걷는다. 안 걷으면 죽은 자리에 이름표만 남는다.
	for (auto It = WidgetsBySeat.CreateIterator(); It; ++It)
	{
		if (LiveSeats.Contains(It.Key()) && It.Value())
		{
			continue;
		}

		const int32 GoneSeat = It.Key();

		if (It.Value())
		{
			It.Value()->DestroyComponent();
		}

		if (UWidgetComponent* Detail = DetailsBySeat.FindRef(GoneSeat))
		{
			Detail->DestroyComponent();
		}
		DetailsBySeat.Remove(GoneSeat);

		if (VisibleDetailSeat == GoneSeat)
		{
			VisibleDetailSeat = INDEX_NONE;
		}

		BoundHealthASCs.Remove(GoneSeat);
		PlateOpacityBySeat.Remove(GoneSeat);
		It.RemoveCurrent();
	}
}

void UBGSeatInfoDisplayComponent::RefreshWidgets()
{
	const ABGPlayerController* PC = GetOwningController();
	if (!PC)
	{
		return;
	}

	for (FBGSeatView& View : UBGSeatViewLibrary::MakeSeatViews(PC))
	{
		//	연출이 아직 결과를 보여줄 때가 아니면 바뀌기 전 체력을 보여준다.
		if (const int32* Held = HeldHealthBySeat.Find(View.SeatIndex))
		{
			View.Health = *Held;
		}

		//	본인 것은 머리 위가 아니라 좌하단 턴 패널이 맡는다.
		if (View.bIsLocalPlayer)
		{
			if (UBGTurnPanelWidget* Panel = PC->GetTurnPanelWidget())
			{
				Panel->SetLocalSeatInfo(View);
			}
			continue;
		}

		const TObjectPtr<UWidgetComponent>* Found = WidgetsBySeat.Find(View.SeatIndex);
		if (!Found || !*Found)
		{
			continue;
		}

		if (UBGSeatInfoWidget* Info = Cast<UBGSeatInfoWidget>((*Found)->GetUserWidgetObject()))
		{
			Info->SetSeatInfo(View);
		}

		//	상세도 같은 값으로 맞춘다. 안 그러면 커서를 올려 둔 채로 체력이 깎였을 때
		//	이름표만 바뀌고 상세는 옛 값을 보여준다.
		if (UWidgetComponent* DetailComp = DetailsBySeat.FindRef(View.SeatIndex))
		{
			if (UBGSeatDetailWidget* Detail = Cast<UBGSeatDetailWidget>(DetailComp->GetUserWidgetObject()))
			{
				Detail->SetSeatInfo(View);
			}
		}
	}
}

//	화면 가장자리가 아니라 그 캐릭터 위에 띄운다. 대상과 정보가 떨어져 있으면
//	누구 것인지 바로 안 읽힌다.
void UBGSeatInfoDisplayComponent::ShowDetailFor(int32 SeatIndex)
{
	HideDetail();

	//	이름표가 걷혀 있는 좌석에는 상세도 열지 않는다 — 상세만 떠 있으면 어색하다.
	if (SeatIndex == INDEX_NONE || GetTargetPlateOpacity(SeatIndex) <= 0.f)
	{
		return;
	}

	if (UWidgetComponent* Detail = DetailsBySeat.FindRef(SeatIndex))
	{
		Detail->SetVisibility(true);
		VisibleDetailSeat = SeatIndex;
	}
}

void UBGSeatInfoDisplayComponent::HideDetail()
{
	if (VisibleDetailSeat == INDEX_NONE)
	{
		return;
	}

	if (UWidgetComponent* Detail = DetailsBySeat.FindRef(VisibleDetailSeat))
	{
		Detail->SetVisibility(false);
	}
	VisibleDetailSeat = INDEX_NONE;
}

void UBGSeatInfoDisplayComponent::UpdateHover()
{
	const ABGPlayerController* PC = GetOwningController();
	if (!PC)
	{
		return;
	}

	int32 NewSeat = INDEX_NONE;

	//	Visibility 채널로는 캐릭터를 못 맞힌다. 엔진 기본 프로파일 Pawn 과 CharacterMesh 가
	//	둘 다 그 채널을 Ignore 로 두기 때문이다(BaseEngine.ini). 그래서 Pawn 채널로 쏜다.
	//
	//	GetHitResultUnderCursor* 를 안 쓰는 이유: 무시 목록을 넘길 수 없다. 카메라가 자기
	//	캡슐 안에 있어서, 그대로 쏘면 어디를 가리키든 자기 자신이 먼저 걸린다.
	FVector Origin;
	FVector Direction;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return;   //	커서가 화면 밖이거나 아직 없다.
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BGSeatHover), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(PC->GetPawn());

	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(
			Hit, Origin, Origin + Direction * HoverTraceDistance, ECC_Pawn, Params))
	{
		//	캡슐이 맞든 메시가 맞든 주인은 같은 캐릭터다 — 컴포넌트가 아니라 액터로 판단한다.
		if (ABGCharacter* Character = Cast<ABGCharacter>(Hit.GetActor()))
		{
			if (const ABGPlayerState* PS = Character->GetPlayerState<ABGPlayerState>())
			{
				NewSeat = PS->GetSeatIndex();
			}
			else if (const int32* Remembered = SeatByCharacter.Find(Character))
			{
				//	이탈자가 두고 간 폰은 PlayerState 가 비어 있다. 기억해 둔 좌석으로 잡아
				//	상세도 뜨고 대상으로도 고를 수 있게 한다.
				NewSeat = *Remembered;
			}
		}
	}

	if (NewSeat == HoveredSeat)
	{
		return;
	}

	HoveredSeat = NewSeat;

	if (HoveredSeat == INDEX_NONE)
	{
		HideDetail();
	}
	else
	{
		ShowDetailFor(HoveredSeat);
	}

	//	패널을 연 직후 값을 채운다. 다음 통지를 기다리면 한 프레임 빈 채로 보인다.
	RefreshWidgets();

	OnHoveredSeatChanged.Broadcast(HoveredSeat);
}

void UBGSeatInfoDisplayComponent::DestroyAllWidgets()
{
	for (const TPair<int32, TObjectPtr<UWidgetComponent>>& Pair : WidgetsBySeat)
	{
		if (Pair.Value) { Pair.Value->DestroyComponent(); }
	}
	WidgetsBySeat.Reset();

	for (const TPair<int32, TObjectPtr<UWidgetComponent>>& Pair : DetailsBySeat)
	{
		if (Pair.Value) { Pair.Value->DestroyComponent(); }
	}
	DetailsBySeat.Reset();
	SeatByCharacter.Reset();

	VisibleDetailSeat = INDEX_NONE;
}
