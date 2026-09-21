#include "Ping/BGPingDisplayComponent.h"

#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Chating/BGChatingSubsystem.h"
#include "Components/WidgetComponent.h"
#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Ping/BGPingComponent.h"
#include "TimerManager.h"
#include "UI/Widget/BGPingMarkerWidget.h"

namespace
{
	/** 좌석 팔레트 — 직업 색이 없을 때 쓴다. 7인이 최대라 7색이면 충분하다. */
	const FLinearColor GSeatPalette[7] = {
		FLinearColor(0.90f, 0.20f, 0.20f), FLinearColor(0.20f, 0.45f, 0.95f),
		FLinearColor(0.15f, 0.75f, 0.35f), FLinearColor(0.95f, 0.70f, 0.10f),
		FLinearColor(0.65f, 0.30f, 0.90f), FLinearColor(0.10f, 0.75f, 0.75f),
		FLinearColor(0.95f, 0.45f, 0.10f)
	};
}

UBGPingDisplayComponent::UBGPingDisplayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBGPingDisplayComponent::BeginPlay()
{
	Super::BeginPlay();

	//	클라에서는 BeginPlay 시점에 아직 로컬 컨트롤러가 아니다(Player 가 나중에 붙는다).
	//	여기서 걸러내면 영영 구독하지 못한다 — 판정은 TryBindGameState 로 미룬다.
	TryBindGameState();

	if (!BoundGameState.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(BindRetryTimer, this,
				&UBGPingDisplayComponent::TryBindGameState, 0.5f, true);
		}
	}
}

void UBGPingDisplayComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BindRetryTimer);
	}

	TArray<int32> Seats;
	MarkersBySender.GetKeys(Seats);
	for (int32 Seat : Seats)
	{
		RemoveMarker(Seat);
	}

	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnPingReceived.RemoveAll(this);
	}
	BoundGameState.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBGPingDisplayComponent::TryBindGameState()
{
	if (BoundGameState.IsValid())
	{
		return;
	}

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	UWorld* World = GetWorld();
	ABGGameState* GS = World ? World->GetGameState<ABGGameState>() : nullptr;
	if (!GS)
	{
		return;
	}

	GS->OnPingReceived.AddUObject(this, &UBGPingDisplayComponent::HandlePingReceived);
	BoundGameState = GS;

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 표시 구독 완료 (NetMode %d)"), static_cast<int32>(World->GetNetMode()));

	World->GetTimerManager().ClearTimer(BindRetryTimer);
}

void UBGPingDisplayComponent::HandlePingReceived(const FBGPingEvent& Event)
{
	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 수신 %d → %d : %s (t=%.2f)"),
		Event.SenderSeat, Event.TargetSeat, *Event.PingTag.ToString(), Event.ServerTime);

	AddChatLine(Event);
	ShowMarker(Event);
}

void UBGPingDisplayComponent::AddChatLine(const FBGPingEvent& Event) const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UBGChatingSubsystem* Chat = GI ? GI->GetSubsystem<UBGChatingSubsystem>() : nullptr;
	if (!Chat)
	{
		return;
	}

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	const UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	const FBGPingDef* Def = PingComp ? PingComp->FindDef(Event.PingTag) : BGPing::FindDefaultDef(Event.PingTag);

	const FString Label = Def ? Def->Label.ToString() : Event.PingTag.ToString();
	const FString Line = (Event.TargetSeat == INDEX_NONE)
		? FString::Printf(TEXT("%d번: %s"), Event.SenderSeat + 1, *Label)
		: FString::Printf(TEXT("%d번 → %d번: %s"), Event.SenderSeat + 1, Event.TargetSeat + 1, *Label);

	Chat->AddPingEntry(Line, Event.PingTag, Event.SenderSeat, Event.TargetSeat);
}

ABGCharacter* UBGPingDisplayComponent::GetSeatCharacter(int32 Seat) const
{
	const ABGGameState* GS = BoundGameState.Get();
	if (!GS || Seat == INDEX_NONE)
	{
		return nullptr;
	}

	const ABGPlayerState* PS = const_cast<ABGGameState*>(GS)->GetPlayerStateBySeat(Seat);
	return PS ? Cast<ABGCharacter>(PS->GetPawn()) : nullptr;
}

FLinearColor UBGPingDisplayComponent::GetSeatColor(int32 Seat) const
{
	if (Seat == INDEX_NONE)
	{
		return FLinearColor::White;
	}

	const ABGCharacter* Character = GetSeatCharacter(Seat);
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;

	if (Character && Data)
	{
		if (const FBGCharacterRow* Row = Data->GetCharacterRow(Character->GetJobTag()))
		{
			//	White 는 "아직 안 정했다" 는 뜻이다 — 그때만 좌석 팔레트로 내려간다.
			if (!Row->PlayerColor.Equals(FLinearColor::White))
			{
				return Row->PlayerColor;
			}
		}
	}

	return GSeatPalette[FMath::Abs(Seat) % UE_ARRAY_COUNT(GSeatPalette)];
}

void UBGPingDisplayComponent::ShowMarker(const FBGPingEvent& Event)
{
	if (!MarkerWidgetClass)
	{
		return;
	}

	const int32 AnchorSeat = (Event.TargetSeat != INDEX_NONE) ? Event.TargetSeat : Event.SenderSeat;
	ABGCharacter* Character = GetSeatCharacter(AnchorSeat);
	if (!Character)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 좌석 %d 캐릭터가 없어 마커 생략"), AnchorSeat);
		return;
	}

	//	같은 사람이 다시 보내면 앞의 것을 지운다. 상한을 넘으면 가장 오래된 것부터 지운다.
	RemoveMarker(Event.SenderSeat);
	while (MarkersBySender.Num() >= MaxMarkers)
	{
		TArray<int32> Seats;
		MarkersBySender.GetKeys(Seats);
		RemoveMarker(Seats[0]);
	}

	UWidgetComponent* WidgetComp = NewObject<UWidgetComponent>(Character);
	if (!WidgetComp)
	{
		return;
	}

	WidgetComp->SetupAttachment(Character->GetRootComponent());
	WidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	WidgetComp->SetDrawSize(MarkerDrawSize);
	WidgetComp->SetWidgetClass(MarkerWidgetClass);
	WidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComp->SetGenerateOverlapEvents(false);
	WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, MarkerHeightZ));
	WidgetComp->SetPivot(FVector2D(0.5f, 1.f));
	WidgetComp->RegisterComponent();
	WidgetComp->InitWidget();

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	const UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	const FBGPingDef* Def = PingComp ? PingComp->FindDef(Event.PingTag) : BGPing::FindDefaultDef(Event.PingTag);

	if (UBGPingMarkerWidget* Marker = Cast<UBGPingMarkerWidget>(WidgetComp->GetUserWidgetObject()))
	{
		UTexture2D* Icon = Def ? Def->Icon.Get() : nullptr;
		const FLinearColor IconColor = Def ? Def->IconColor : FLinearColor::White;
		Marker->SetPing(Icon, IconColor, GetSeatColor(Event.SenderSeat));
	}

	FBGPingMarker& Entry = MarkersBySender.Add(Event.SenderSeat);
	Entry.Widget = WidgetComp;

	const float Lifetime = (Def && Def->LifetimeSec > 0.f) ? Def->LifetimeSec : 4.f;
	if (UWorld* World = GetWorld())
	{
		const int32 SenderSeat = Event.SenderSeat;
		World->GetTimerManager().SetTimer(Entry.ExpireTimer,
			FTimerDelegate::CreateWeakLambda(this, [this, SenderSeat]() { RemoveMarker(SenderSeat); }),
			Lifetime, false);
	}

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 마커 표시 좌석 %d (발신 %d, %.1f초)"),
		AnchorSeat, Event.SenderSeat, Lifetime);
}

void UBGPingDisplayComponent::RemoveMarker(int32 SenderSeat)
{
	FBGPingMarker Entry;
	if (!MarkersBySender.RemoveAndCopyValue(SenderSeat, Entry))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Entry.ExpireTimer);
	}

	if (Entry.Widget)
	{
		Entry.Widget->DestroyComponent();
	}
}
