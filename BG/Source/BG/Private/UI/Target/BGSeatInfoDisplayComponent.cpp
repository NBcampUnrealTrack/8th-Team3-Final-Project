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
		BoundGameState = GS;
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
	}
	BoundGameState = nullptr;

	//	체력 구독은 ASC 가 사라지면 함께 없어진다. 목록만 비운다.
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

void UBGSeatInfoDisplayComponent::HandleHealthChanged(const FOnAttributeChangeData& /*Data*/)
{
	RefreshWidgets();
}

void UBGSeatInfoDisplayComponent::BindHealthDelegate(ABGCharacter* Character, int32 SeatIndex)
{
	UAbilitySystemComponent* ASC =
		UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Character);
	if (!ASC)
	{
		return;   //	폰은 있는데 ASC 가 아직 없다. 다음 구조 확인에서 다시 본다.
	}

	if (BoundHealthASCs.FindRef(SeatIndex).Get() == ASC)
	{
		return;
	}

	//	Health 와 MaxHealth 둘 다 본다. 최대치만 바뀌는 경우(직업 배정)도 표시가 달라진다.
	ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UBGSeatInfoDisplayComponent::HandleHealthChanged);
	ASC->GetGameplayAttributeValueChangeDelegate(UBGAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UBGSeatInfoDisplayComponent::HandleHealthChanged);

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

	UWidgetComponent* Widget = MakeWidgetOn(Character, InfoWidgetClass, WidgetDrawSize,
		AnchorZ, FVector2D(0.5f, 1.f));
	if (!Widget)
	{
		return nullptr;
	}
	WidgetsBySeat.Add(SeatIndex, Widget);

	//	상세는 이름표 위에 얹는다. 평소에는 숨어 있다가 커서를 올렸을 때만 켜진다.
	if (DetailWidgetClass && DetailDrawSize.Y > 0.f)
	{
		//	이름표 높이와 틈만큼 Pivot 을 더 밀어 올린다. 크기가 바뀌어도 간격이 따라온다.
		const float LiftRatio = (WidgetDrawSize.Y + DetailGap) / DetailDrawSize.Y;

		if (UWidgetComponent* Detail = MakeWidgetOn(Character, DetailWidgetClass, DetailDrawSize,
				AnchorZ, FVector2D(0.5f, 1.f + LiftRatio)))
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

		//	이탈자가 두고 간 폰은 PlayerState 가 없다. 그 자리는 비워 둔다 —
		//	없는 사람의 정보를 띄우는 것보다 낫다.
		const ABGPlayerState* PS = Character ? Character->GetPlayerState<ABGPlayerState>() : nullptr;
		if (!PS || PS->GetSeatIndex() == INDEX_NONE)
		{
			continue;
		}

		//	자기 머리 위에는 띄우지 않는다. 자기 정보는 화면 좌하단 전용 패널이 맡는다.
		if (PS == LocalState)
		{
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

	for (const FBGSeatView& View : UBGSeatViewLibrary::MakeSeatViews(PC))
	{
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

	if (SeatIndex == INDEX_NONE)
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
		if (const ABGCharacter* Character = Cast<ABGCharacter>(Hit.GetActor()))
		{
			if (const ABGPlayerState* PS = Character->GetPlayerState<ABGPlayerState>())
			{
				NewSeat = PS->GetSeatIndex();
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

	VisibleDetailSeat = INDEX_NONE;
}
