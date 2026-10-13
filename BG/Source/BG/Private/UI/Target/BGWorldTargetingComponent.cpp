#include "UI/Target/BGWorldTargetingComponent.h"

#include "Card/BGDataSubsystem.h"
#include "Components/InputComponent.h"
#include "Components/Widget.h"
#include "Core/BGCharacter.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"
#include "FX/BGPresentationDataAsset.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UI/Target/BGGameplayHudOwner.h"
#include "UI/Target/BGSeatInfoDisplayComponent.h"
#include "UI/Target/BGTargetHighlight.h"
#include "UI/View/BGSeatViewLibrary.h"

UBGWorldTargetingComponent::UBGWorldTargetingComponent()
{
	//	조준 중에만 돈다. 끝난 뒤에도 카메라가 정면으로 돌아올 때까지는 계속 돈다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

ABGPlayerController* UBGWorldTargetingComponent::GetOwningController() const
{
	return Cast<ABGPlayerController>(GetOwner());
}

UBGSeatInfoDisplayComponent* UBGWorldTargetingComponent::GetInfoDisplay() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UBGSeatInfoDisplayComponent>() : nullptr;
}

void UBGWorldTargetingComponent::BeginPlay()
{
	Super::BeginPlay();

	const ABGPlayerController* PC = GetOwningController();
	if (!PC || !PC->HasLocalViewer())
	{
		//	조준은 보는 사람 화면의 일이다. 서버가 남의 컨트롤러에 대해 돌릴 이유가 없다.
		SetComponentTickEnabled(false);
		return;
	}

	TryBindConfirmInput();
}

void UBGWorldTargetingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bTargeting)
	{
		//	통지 없이 상태만 되돌린다 — 월드가 내려가는 중이라 받을 쪽이 없다.
		bTargeting = false;
		ApplyTargetingState(false);
	}

	Super::EndPlay(EndPlayReason);
}

void UBGWorldTargetingComponent::BeginTargeting(const FBGTargetRequest& Request)
{
	const ABGPlayerController* PC = GetOwningController();
	if (!PC || !PC->HasLocalViewer())
	{
		return;
	}

	//	이미 고르는 중이면 이전 요청을 접고 시작한다. 안 접으면 ContextId 가 뒤엉킨다.
	if (bTargeting)
	{
		const int32 PreviousContext = ActiveRequest.ContextId;
		bTargeting = false;
		ClearHighlights();
		StopTargetSelectionEffect(true);
		OnTargetSelectionCancelled.Broadcast(PreviousContext);
	}

	//	InputComponent 는 컴포넌트보다 늦게 생긴다. 여기서 한 번 더 시도해 둔다.
	TryBindConfirmInput();

	ActiveRequest = Request;

	if (ActiveRequest.SelectableSeats.IsEmpty())
	{
		//	고를 것이 없으면 시작하지 않고 즉시 취소한다 — 조준 화면에 갇히지 않게 한다.
		UE_LOG(LogBGCard, Log, TEXT("[Target] 후보 좌석이 없어 조준을 시작하지 않는다"));
		OnTargetSelectionCancelled.Broadcast(ActiveRequest.ContextId);
		return;
	}

	bTargeting = true;
	ApplyTargetingState(true);
	RefreshHighlights();

	UE_LOG(LogBGCard, Log, TEXT("[Target] 조준 시작 — 후보 %d명 (Context=%d)"),
		ActiveRequest.SelectableSeats.Num(), ActiveRequest.ContextId);
}

void UBGWorldTargetingComponent::CancelTargeting()
{
	if (!bTargeting)
	{
		return;
	}

	const int32 ContextId = ActiveRequest.ContextId;

	bTargeting = false;
	ApplyTargetingState(false);

	UE_LOG(LogBGCard, Log, TEXT("[Target] 조준 취소 (Context=%d)"), ContextId);

	OnTargetSelectionCancelled.Broadcast(ContextId);
}

void UBGWorldTargetingComponent::NotifyConfirmPressed()
{
	if (!bTargeting)
	{
		return;
	}

	const UBGSeatInfoDisplayComponent* Display = GetInfoDisplay();
	const int32 Seat = Display ? Display->GetHoveredSeat() : INDEX_NONE;

	//	빈 곳이나 후보가 아닌 캐릭터를 눌렀다. 조준은 그대로 열어 둔다 —
	//	여기서 취소해 버리면 살짝 빗나간 클릭에 카드가 손패로 돌아간다.
	if (!IsSelectableSeat(Seat))
	{
		return;
	}

	const int32 ContextId = ActiveRequest.ContextId;

	bTargeting = false;
	ApplyTargetingState(false);

	UE_LOG(LogBGCard, Log, TEXT("[Target] 대상 확정 — 좌석 %d (Context=%d)"), Seat, ContextId);

	OnSeatSelected.Broadcast(Seat, ContextId);
}

void UBGWorldTargetingComponent::ApplyTargetingState(bool bEnter)
{
	if (bEnter)
	{
		SetHudClickThrough(true);

		//	호버 판정이 켜져 있어야 무엇을 고르는지 알 수 있다.
		if (UBGSeatInfoDisplayComponent* Display = GetInfoDisplay())
		{
			Display->SetDisplayEnabled(true);
		}

		SetComponentTickEnabled(true);
		return;
	}

	ClearHighlights();
	StopTargetSelectionEffect(true);
	SetHudClickThrough(false);

	//	카메라가 정면으로 돌아올 때까지는 틱을 계속 돌려야 한다.
	if (FMath::IsNearlyZero(PanYaw))
	{
		SetComponentTickEnabled(false);
	}
}

void UBGWorldTargetingComponent::SetHudClickThrough(bool bClickThrough)
{
	if (!bClickThrough)
	{
		for (const TPair<TObjectPtr<UWidget>, ESlateVisibility>& Pair : SavedHudVisibility)
		{
			if (Pair.Key) { Pair.Key->SetVisibility(Pair.Value); }
		}
		SavedHudVisibility.Reset();
		return;
	}

	const IBGGameplayHudOwner* HudOwner = Cast<IBGGameplayHudOwner>(GetOwner());
	if (!HudOwner)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	HudOwner->CollectGameplayHudWidgets(Widgets);

	SavedHudVisibility.Reset();
	for (UWidget* Widget : Widgets)
	{
		//	이미 안 보이는 위젯은 그대로 둔다. HitTestInvisible 로 바꾸면 접어 둔 것이 켜진다.
		if (!Widget || !Widget->IsVisible())
		{
			continue;
		}

		//	Collapsed 가 아니라 HitTestInvisible 이다 — 조준 중에도 손패는 보여야 한다.
		SavedHudVisibility.Add(Widget, Widget->GetVisibility());
		Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UBGWorldTargetingComponent::TryBindConfirmInput()
{
	ABGPlayerController* PC = GetOwningController();
	if (bConfirmInputBound || !PC || !PC->InputComponent)
	{
		return;
	}

	//	조준 중이 아니면 NotifyConfirmPressed 가 스스로 빠져나가므로 평소 좌클릭에는 영향이 없다.
	PC->InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,
		this, &UBGWorldTargetingComponent::NotifyConfirmPressed);

	bConfirmInputBound = true;
}

void UBGWorldTargetingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bTargeting)
	{
		RefreshHighlights();
		UpdateEdgePan(DeltaTime);
		return;
	}

	UpdateRecenter(DeltaTime);
}

bool UBGWorldTargetingComponent::IsSelectableSeat(int32 SeatIndex) const
{
	return SeatIndex != INDEX_NONE && ActiveRequest.SelectableSeats.Contains(SeatIndex);
}

ABGCharacter* UBGWorldTargetingComponent::FindCharacterBySeat(int32 SeatIndex) const
{
	return UBGSeatViewLibrary::FindCharacterBySeat(GetOwningController(), SeatIndex);
}

void UBGWorldTargetingComponent::RefreshHighlights()
{
	const UBGSeatInfoDisplayComponent* Display = GetInfoDisplay();
	const int32 AimedSeat = Display ? Display->GetHoveredSeat() : INDEX_NONE;

	//	매번 목록을 다시 만든다. 조준 도중 대상이 죽거나 나갈 수 있어
	//	"시작할 때 모아 둔 목록" 을 믿으면 사라진 캐릭터에 값을 쓴다.
	ClearHighlights();

	for (const int32 Seat : ActiveRequest.SelectableSeats)
	{
		ABGCharacter* Character = FindCharacterBySeat(Seat);
		if (!Character)
		{
			continue;
		}

		BGTargetHighlight::Apply(Character, (Seat == AimedSeat)
			? BGTargetHighlight::EState::Aimed
			: BGTargetHighlight::EState::Selectable);

		HighlightedCharacters.Add(Character);
	}

	UpdateTargetSelectionEffect(IsSelectableSeat(AimedSeat) ? AimedSeat : INDEX_NONE);
}

void UBGWorldTargetingComponent::ClearHighlights()
{
	for (const TWeakObjectPtr<ABGCharacter>& Weak : HighlightedCharacters)
	{
		if (ABGCharacter* Character = Weak.Get())
		{
			BGTargetHighlight::Apply(Character, BGTargetHighlight::EState::None);
		}
	}
	HighlightedCharacters.Reset();
}

//대상 선택 이펙트 획득 함수
UNiagaraSystem* UBGWorldTargetingComponent::ResolveTargetSelectionSystem() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UBGDataSubsystem>() : nullptr;
	const UBGPresentationDataAsset* Presentation = Data ? Data->GetPresentationData() : nullptr;
	if (!Presentation)
	{
		return nullptr;
	}

	const TSoftObjectPtr<UNiagaraSystem>& Styled =
		Presentation->GetTargetSelectionEffect(ActiveRequest.PresentationStyle);
	if (!Styled.IsNull())
	{
		return Styled.LoadSynchronous();
	}

	// 개별 계열이 아직 배정되지 않았어도 대상선 자체는 보이게 Basic으로 폴백한다.
	return Presentation->BasicTargetSelectionEffect.LoadSynchronous();
}

//대상 선택 이펙트 업데이트 함수, 위치와 보임 여부 업데이트
void UBGWorldTargetingComponent::UpdateTargetSelectionEffect(int32 AimedSeat)
{
	ABGCharacter* Target = IsSelectableSeat(AimedSeat) ? FindCharacterBySeat(AimedSeat) : nullptr;
	ABGCharacter* Source = GetOwningController()
		? Cast<ABGCharacter>(GetOwningController()->GetPawn())
		: nullptr;

	if (!Source || !Target)
	{
		// 호버가 잠시 끊겼다고 시스템을 매 프레임 재생성하지 않는다. 다시 유효한 대상에
		// 올라오면 같은 컴포넌트를 보이고 EndPosition만 바꾼다.
		if (TargetSelectionEffect)
		{
			TargetSelectionEffect->SetVisibility(false, true);
		}
		return;
	}

	if (!TargetSelectionEffect)
	{
		UNiagaraSystem* System = ResolveTargetSelectionSystem();
		if (!System)
		{
			return;
		}

		const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		const UBGDataSubsystem* Data = GameInstance
			? GameInstance->GetSubsystem<UBGDataSubsystem>()
			: nullptr;
		const float GlobalZ = Data ? Data->GetGlobalFXSpawnZOffset() : 0.f;

		// 이 시스템의 이미터는 World Space다. 시스템 원점과 EndPosition을 같은 World +Z만큼
		// 올린다. User.EffectZOffset은 이미터 내부 원점 보정이므로 코드에서 덮어쓰지 않는다.
		TargetSelectionEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			System,
			Source->GetActorLocation() + FVector::UpVector * GlobalZ,
			Source->GetActorRotation(),
			FVector::OneVector,
			/*bAutoDestroy=*/false,
			/*bAutoActivate=*/false);

		if (!TargetSelectionEffect)
		{
			return;
		}

		TargetSelectionEffect->SetVariableBool(TEXT("User.bIsEnding"), false);
		TargetSelectionEffect->SetVariablePosition(
			TEXT("User.EndPosition"), Target->GetActorLocation() + FVector::UpVector * GlobalZ);
		TargetSelectionEffect->Activate(true);
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UBGDataSubsystem>() : nullptr;
	const float GlobalZ = Data ? Data->GetGlobalFXSpawnZOffset() : 0.f;
	TargetSelectionEffect->SetVariablePosition(
		TEXT("User.EndPosition"), Target->GetActorLocation() + FVector::UpVector * GlobalZ);
	TargetSelectionEffect->SetVisibility(true, true);
}

//대상 선택 이펙트 종료 함수
void UBGWorldTargetingComponent::StopTargetSelectionEffect(bool bPlayEndAnimation)
{
	UNiagaraComponent* Effect = TargetSelectionEffect;
	TargetSelectionEffect = nullptr;
	if (!Effect)
	{
		return;
	}

	// 마지막 유효 대상이 보이던 상태에서만 종료 연출을 재생한다. 호버가 끊겨 숨겨진
	// 선을 취소 순간 다시 드러내면 이전 대상 쪽으로 잘못 튀는 것처럼 보인다.
	if (!bPlayEndAnimation || !Effect->IsVisible())
	{
		Effect->DestroyComponent();
		return;
	}

	Effect->SetVariableBool(TEXT("User.bIsEnding"), true);

	float EndDelay = 1.f;
	if (const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UBGDataSubsystem* Data = GameInstance->GetSubsystem<UBGDataSubsystem>())
		{
			if (const UBGPresentationDataAsset* Presentation = Data->GetPresentationData())
			{
				EndDelay = Presentation->TargetSelectionEndDelaySeconds;
			}
		}
	}

	if (EndDelay <= 0.f || !GetWorld())
	{
		Effect->DestroyComponent();
		return;
	}

	const TWeakObjectPtr<UNiagaraComponent> WeakEffect = Effect;
	EndingSelectionEffects.RemoveAll([](const TWeakObjectPtr<UNiagaraComponent>& Item) { return !Item.IsValid(); });
	EndingSelectionEffects.Add(WeakEffect);
	FTimerHandle CleanupTimer;
	GetWorld()->GetTimerManager().SetTimer(
		CleanupTimer,
		FTimerDelegate::CreateWeakLambda(this, [WeakEffect]()
		{
			if (UNiagaraComponent* Component = WeakEffect.Get())
			{
				Component->DestroyComponent();
			}
		}),
		EndDelay,
		false);
}

// 판정 UI가 시작되면 남은 대상 선택 종료 이펙트를 즉시 정리한다.
void UBGWorldTargetingComponent::ClearSelectionEffectsForResolution()
{
	StopTargetSelectionEffect(false);
	for (const TWeakObjectPtr<UNiagaraComponent>& WeakEffect : EndingSelectionEffects)
	{
		if (UNiagaraComponent* Effect = WeakEffect.Get()) Effect->DestroyComponent();
	}
	EndingSelectionEffects.Reset();
}

void UBGWorldTargetingComponent::UpdateEdgePan(float DeltaTime)
{
	const ABGPlayerController* PC = GetOwningController();
	if (!PC || EdgePanMargin <= 0.f)
	{
		return;
	}

	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!PC->GetMousePosition(MouseX, MouseY))
	{
		return;   //	커서가 화면 밖이거나 아직 없다.
	}

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0)
	{
		return;
	}

	//	가장자리에 얼마나 깊이 들어왔는지를 0~1 로 환산한다. 끝에 붙을수록 빨리 돈다.
	const float Ratio  = MouseX / static_cast<float>(ViewportX);
	float       Amount = 0.f;

	if (Ratio < EdgePanMargin)
	{
		Amount = -(EdgePanMargin - Ratio) / EdgePanMargin;
	}
	else if (Ratio > 1.f - EdgePanMargin)
	{
		Amount = (Ratio - (1.f - EdgePanMargin)) / EdgePanMargin;
	}

	if (FMath::IsNearlyZero(Amount))
	{
		return;
	}

	PanYaw += Amount * EdgePanSpeedDeg * DeltaTime;

	if (MaxPanYawDeg > 0.f)
	{
		PanYaw = FMath::Clamp(PanYaw, -MaxPanYawDeg, MaxPanYawDeg);
	}

	ApplyPanYaw();
}

void UBGWorldTargetingComponent::UpdateRecenter(float DeltaTime)
{
	if (FMath::IsNearlyZero(PanYaw))
	{
		SetComponentTickEnabled(false);
		return;
	}

	PanYaw = FMath::FInterpConstantTo(PanYaw, 0.f, DeltaTime, RecenterSpeedDeg);
	ApplyPanYaw();
}

void UBGWorldTargetingComponent::ApplyPanYaw()
{
	ABGPlayerController* PC = GetOwningController();
	ABGCharacter* Character = PC ? Cast<ABGCharacter>(PC->GetPawn()) : nullptr;
	if (!Character)
	{
		return;
	}

	//	기준 자세 캡처는 ABGCharacter 쪽에 있다 — UBGCameraLookComponent 와 공유한다.
	Character->SetPrimaryCameraYawOffset(PanYaw);
}
