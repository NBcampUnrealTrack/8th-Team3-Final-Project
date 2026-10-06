#include "Ping/BGPingDisplayComponent.h"

#include "Audio/BGAudioSubsystem.h"

#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Chating/BGChatingSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "HAL/IConsoleManager.h"
#include "Ping/BGPingComponent.h"
#include "TimerManager.h"
#include "UI/Widget/BGPingMarkerWidget.h"

namespace
{
	//	범위 밖 값은 "지정 안 함" — 컴포넌트 UPROPERTY 값을 그대로 쓴다.
	TAutoConsoleVariable<float> CVarPingMarkerZ(TEXT("BG.Ping.MarkerZ"), -1.f,
		TEXT("핑 마커를 거는 월드 높이(cm). 0 이하면 컴포넌트 값."));
	TAutoConsoleVariable<float> CVarPingSpacing(TEXT("BG.Ping.Spacing"), -1.f,
		TEXT("같은 좌석 핑 마커 좌우 화면 간격(슬레이트 단위). 0 미만이면 컴포넌트 값."));
	TAutoConsoleVariable<float> CVarPingLift(TEXT("BG.Ping.Lift"), -1.f,
		TEXT("거는 지점에서 화면 위로 올리는 양(슬레이트 단위). 0 미만이면 컴포넌트 값."));
	TAutoConsoleVariable<float> CVarPingOffsetX(TEXT("BG.Ping.OffsetX"), -10000.f,
		TEXT("스택 가로 보정(슬레이트 단위). -9999 미만이면 컴포넌트 값."));
	TAutoConsoleVariable<float> CVarPingTilt(TEXT("BG.Ping.Tilt"), -1000.f,
		TEXT("슬롯당 기울기(도). -360 미만이면 컴포넌트 값."));
	TAutoConsoleVariable<float> CVarPingScale(TEXT("BG.Ping.Scale"), -1.f,
		TEXT("핑 마커 화면 배율. 0 이하면 컴포넌트 값."));

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<float> CVarPingDuration(TEXT("BG.Ping.Duration"), -1.f,
		TEXT("핑 마커 수명(초) 강제. 0 이하면 DT 값(캡처용)."));
#endif
}

UBGPingDisplayComponent::UBGPingDisplayComponent()
{
	//	마커가 떠 있는 동안만 돈다 — 만료·페이드·좌석 폰 교체를 여기서 본다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
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

	for (int32 Index = ActiveMarkers.Num() - 1; Index >= 0; --Index)
	{
		RemoveMarkerAt(Index);
	}

	if (ABGGameState* GS = BoundGameState.Get())
	{
		GS->OnPingReceived.RemoveAll(this);
	}
	BoundGameState.Reset();

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	if (UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr)
	{
		PingComp->OnIconsLoaded.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UBGPingDisplayComponent::TryBindGameState()
{
	if (BoundGameState.IsValid())
	{
		return;
	}

	ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	if (!PC || !PC->HasLocalViewer())
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

	if (UBGPingComponent* PingComp = PC->GetPingComponent(); PingComp && !bIconsBound)
	{
		PingComp->OnIconsLoaded.AddUObject(this, &UBGPingDisplayComponent::HandleIconsLoaded);
		bIconsBound = true;
	}

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 표시 구독 완료 (NetMode %d)"), static_cast<int32>(World->GetNetMode()));

	World->GetTimerManager().ClearTimer(BindRetryTimer);
}

void UBGPingDisplayComponent::HandlePingReceived(const FBGPingEvent& Event)
{
	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 수신 %d → %d : %s (t=%.2f)"),
		Event.SenderSeat, Event.TargetSeat, *Event.PingTag.ToString(), Event.ServerTime);

	AddChatLine(Event);
	ShowMarker(Event);
	PlayPingSound(Event);
}

// 핑 사운드 재생
void UBGPingDisplayComponent::PlayPingSound(const FBGPingEvent& Event) const
{
	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	if (!PC || !PC->IsLocalController() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	const UBGPingComponent* PingComp = PC->GetPingComponent();
	const FBGPingDef* Def = PingComp ? PingComp->FindDef(Event.PingTag) : BGPing::FindDefaultDef(Event.PingTag);
	if (Def && Def->AudioTag.IsValid())
	{
		if (UBGAudioSubsystem* Audio = UBGAudioSubsystem::Get(this))
		{
			Audio->Play(Def->AudioTag);
		}
	}
}

void UBGPingDisplayComponent::HandleIconsLoaded()
{
	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	const UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	if (!PingComp)
	{
		return;
	}

	int32 Refreshed = 0;
	for (const FBGPingMarker& Marker : ActiveMarkers)
	{
		const FBGPingDef* Def = PingComp->FindDef(Marker.PingTag);
		UBGPingMarkerWidget* Widget = IsValid(Marker.Widget) ? Cast<UBGPingMarkerWidget>(Marker.Widget->GetUserWidgetObject()) : nullptr;
		if (Def && Widget && Def->Icon.Get())
		{
			Widget->SetIconTexture(Def->Icon.Get());
			++Refreshed;
		}
	}
	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 아이콘 선로딩 완료 — 떠 있던 마커 %d개 갱신"), Refreshed);
}

FString UBGPingDisplayComponent::MakeSeatLabel(int32 Seat, const FString& Name)
{
	const FString Tag = FString::Printf(TEXT("P%d"), Seat + 1);
	if (Name.IsEmpty())
	{
		return Tag;
	}

	//	이름 앞 8자. 넘치면 말줄임.
	return FString::Printf(TEXT("%s %s"), *Tag, Name.Len() > 8 ? *(Name.Left(8) + TEXT("…")) : *Name);
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

	//	이름은 서버가 실어 보낸 값이다 — 호스트와 원격 클라가 같은 줄을 남긴다.
	const FString Label = Def ? Def->Label.ToString() : Event.PingTag.ToString();
	const FString Sender = MakeSeatLabel(Event.SenderSeat, Event.SenderName);
	const FString Line = (Event.TargetSeat == INDEX_NONE)
		? FString::Printf(TEXT("%s : %s"), *Sender, *Label)
		: FString::Printf(TEXT("%s → %s : %s"), *Sender, *MakeSeatLabel(Event.TargetSeat, Event.TargetName), *Label);

	Chat->AddPingEntry(Line, Event.PingTag, Event.SenderSeat, Event.TargetSeat, GetSeatColor(Event.SenderSeat));
}

float UBGPingDisplayComponent::GetTunedHeightZ(const AActor* Character) const
{
	const float Override = CVarPingMarkerZ.GetValueOnGameThread();
	if (Override > 0.f)
	{
		return Override;
	}
	if (MarkerHeightZ > 0.f)
	{
		return MarkerHeightZ;
	}

	//	이름표(UBGSeatInfoDisplayComponent)와 같은 지점 — 캡슐 꼭대기 + 40.
	const ACharacter* AsCharacter = Cast<ACharacter>(Character);
	const UCapsuleComponent* Capsule = AsCharacter ? AsCharacter->GetCapsuleComponent() : nullptr;
	return (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f) + 40.f;
}

UBGPingDisplayComponent::FLayout UBGPingDisplayComponent::ReadLayout() const
{
	FLayout L;
	const float Spacing = CVarPingSpacing.GetValueOnGameThread();
	const float Lift = CVarPingLift.GetValueOnGameThread();
	const float OffsetX = CVarPingOffsetX.GetValueOnGameThread();
	const float Tilt = CVarPingTilt.GetValueOnGameThread();
	const float Scale = CVarPingScale.GetValueOnGameThread();
	L.Spacing = (Spacing >= 0.f) ? Spacing : MarkerSpacing;
	L.Lift = (Lift >= 0.f) ? Lift : MarkerLift;
	L.OffsetX = (OffsetX >= -9999.f) ? OffsetX : MarkerOffsetX;
	L.Tilt = (Tilt >= -360.f) ? Tilt : MarkerTiltDeg;
	L.Scale = (Scale > 0.f) ? Scale : MarkerScale;
	return L;
}

ABGCharacter* UBGPingDisplayComponent::GetSeatCharacter(int32 Seat) const
{
	const ABGGameState* GS = BoundGameState.Get();
	if (!GS || Seat == INDEX_NONE)
	{
		return nullptr;
	}

	const ABGPlayerState* PS = GS->GetPlayerStateBySeat(Seat);
	return PS ? Cast<ABGCharacter>(PS->GetPawn()) : nullptr;
}

FLinearColor UBGPingDisplayComponent::GetSeatColor(int32 Seat) const
{
	return BGPing::GetSeatColor(Seat);
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

	//	같은 좌석에 쌓인 것이 한도를 넘으면 그 좌석에서 가장 오래된 것을 먼저 비운다.
	const int32 SeatLimit = FMath::Max(1, MaxMarkersPerSeat);
	int32 SeatCount = 0;
	for (const FBGPingMarker& Marker : ActiveMarkers)
	{
		SeatCount += (Marker.AnchorSeat == AnchorSeat) ? 1 : 0;
	}
	while (SeatCount >= SeatLimit)
	{
		for (int32 Index = ActiveMarkers.Num() - 1; Index >= 0; --Index)
		{
			if (ActiveMarkers[Index].AnchorSeat == AnchorSeat)
			{
				RemoveMarkerAt(Index);
				break;
			}
		}
		--SeatCount;
	}

	//	에디터 값이 0 이어도 무한 루프가 되지 않게 실행 시 한 번 더 막는다.
	const int32 TotalLimit = FMath::Max(1, MaxMarkers);
	while (ActiveMarkers.Num() >= TotalLimit)
	{
		RemoveMarkerAt(ActiveMarkers.Num() - 1);
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
	WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, GetTunedHeightZ(Character)));
	WidgetComp->SetPivot(FVector2D(0.5f, 1.f));
	WidgetComp->RegisterComponent();
	WidgetComp->InitWidget();

	const ABGPlayerController* PC = Cast<ABGPlayerController>(GetOwner());
	const UBGPingComponent* PingComp = PC ? PC->GetPingComponent() : nullptr;
	const FBGPingDef* Def = PingComp ? PingComp->FindDef(Event.PingTag) : BGPing::FindDefaultDef(Event.PingTag);

	if (UBGPingMarkerWidget* Marker = Cast<UBGPingMarkerWidget>(WidgetComp->GetUserWidgetObject()))
	{
		//	아이콘이 아직 로드 전이면 비워 두고 선로딩 완료 때 채운다(HandleIconsLoaded).
		UTexture2D* Icon = Def ? Def->Icon.Get() : nullptr;
		const FLinearColor IconColor = Def ? Def->IconColor : FLinearColor::White;
		const FLinearColor SenderColor = GetSeatColor(Event.SenderSeat);
		Marker->SetPing(Icon, IconColor, SenderColor);
		Marker->SetSenderName(Event.SenderName, Event.SenderSeat + 1, SenderColor);
	}

	float Lifetime = (Def && Def->LifetimeSec > 0.f) ? Def->LifetimeSec : 4.f;
#if !UE_BUILD_SHIPPING
	if (CVarPingDuration.GetValueOnGameThread() > 0.f)
	{
		Lifetime = CVarPingDuration.GetValueOnGameThread();
	}
#endif

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	//	새 것이 가운데(슬롯 0)로 오고 기존 것들이 옆으로 밀린다.
	FBGPingMarker Entry;
	Entry.Widget = WidgetComp;
	Entry.AnchorSeat = AnchorSeat;
	Entry.AnchorActor = Character;
	Entry.PingTag = Event.PingTag;
	Entry.ExpireAt = Now + Lifetime;
	ActiveMarkers.Insert(Entry, 0);

	RepackMarkers();

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 마커 표시 좌석 %d (발신 %d, %.1f초, 좌석 내 %d개)"),
		AnchorSeat, Event.SenderSeat, Lifetime, SeatCount + 1);
}

void UBGPingDisplayComponent::RemoveMarkerAt(int32 Index)
{
	if (!ActiveMarkers.IsValidIndex(Index))
	{
		return;
	}

	UWidgetComponent* Widget = ActiveMarkers[Index].Widget;
	ActiveMarkers.RemoveAt(Index);

	if (IsValid(Widget))
	{
		Widget->DestroyComponent();
	}

	SetComponentTickEnabled(ActiveMarkers.Num() > 0);
}

bool UBGPingDisplayComponent::IsMarkerStale(const FBGPingMarker& Marker, double Now) const
{
	if (!IsValid(Marker.Widget) || !Marker.AnchorActor.IsValid() || Now >= Marker.ExpireAt)
	{
		return true;
	}

	//	좌석의 폰이 바뀌었으면 옛 폰 위 마커는 더 이상 그 좌석을 가리키지 않는다.
	return GetSeatCharacter(Marker.AnchorSeat) != Marker.AnchorActor.Get();
}

void UBGPingDisplayComponent::RepackMarkers()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	//	지울 것은 역순으로 RemoveMarkerAt 을 거쳐 위젯까지 정리한다.
	for (int32 Index = ActiveMarkers.Num() - 1; Index >= 0; --Index)
	{
		if (IsMarkerStale(ActiveMarkers[Index], Now))
		{
			RemoveMarkerAt(Index);
		}
	}

	//	좌석마다 몇 개가 있는지 알아야 스택 전체를 가운데 정렬할 수 있다.
	TMap<int32, int32> CountBySeat;
	for (const FBGPingMarker& Marker : ActiveMarkers)
	{
		++CountBySeat.FindOrAdd(Marker.AnchorSeat);
	}

	const FLayout Layout = ReadLayout();
	TMap<int32, int32> NextSlot;
	for (FBGPingMarker& Marker : ActiveMarkers)
	{
		int32& Slot = NextSlot.FindOrAdd(Marker.AnchorSeat);
		Marker.Slot = Slot++;
		Marker.StackShift = (CountBySeat.FindRef(Marker.AnchorSeat) % 2 == 0) ? -0.5f : 0.f;
		ApplyMarkerTransform(Marker, Now, Layout);
	}

	SetComponentTickEnabled(ActiveMarkers.Num() > 0);
}

void UBGPingDisplayComponent::ApplyMarkerTransform(FBGPingMarker& Marker, double Now, const FLayout& Layout) const
{
	UWidgetComponent* WidgetComp = Marker.Widget;
	if (!IsValid(WidgetComp) || !WidgetComp->IsRegistered())
	{
		return;
	}

	//	만료·파괴 도중에도 불린다 — 루트가 없는 액터를 만지면 그대로 크래시다.
	const AActor* Character = WidgetComp->GetOwner();
	if (!IsValid(Character) || !Character->GetRootComponent())
	{
		return;
	}

	//	0 → 가운데, 1 → 오른쪽, 2 → 왼쪽 … 으로 좌우 교대. 간격·높이는 화면 단위라 거리와 무관하다.
	const int32 Step = (Marker.Slot + 1) / 2;
	const float Side = (Marker.Slot == 0) ? 0.f : ((Marker.Slot % 2 == 1) ? 1.f : -1.f);

	const float Z = GetTunedHeightZ(Character);
	if (!FMath::IsNearlyEqual(Z, Marker.AppliedZ))
	{
		WidgetComp->SetRelativeLocation(FVector(0.f, 0.f, Z));
		Marker.AppliedZ = Z;
	}

	UUserWidget* Widget = WidgetComp->GetUserWidgetObject();
	if (!Widget)
	{
		return;
	}

	const FVector2D Offset(Layout.OffsetX + (Side * Step + Marker.StackShift) * Layout.Spacing, -Layout.Lift);
	const float Angle = Side * Step * Layout.Tilt;
	if (!Offset.Equals(Marker.AppliedOffset) || !FMath::IsNearlyEqual(Layout.Scale, Marker.AppliedScale)
		|| !FMath::IsNearlyEqual(Angle, Marker.AppliedAngle))
	{
		Widget->SetRenderTransform(FWidgetTransform(Offset, FVector2D(Layout.Scale), FVector2D::ZeroVector, Angle));
		Marker.AppliedOffset = Offset;
		Marker.AppliedScale = Layout.Scale;
		Marker.AppliedAngle = Angle;
	}

	//	만료 직전 FadeOutSec 동안 투명해진다.
	const double Remaining = Marker.ExpireAt - Now;
	const float Alpha = (FadeOutSec > 0.f) ? FMath::Clamp(static_cast<float>(Remaining / FadeOutSec), 0.f, 1.f) : 1.f;
	if (!FMath::IsNearlyEqual(Alpha, Marker.AppliedOpacity))
	{
		Widget->SetRenderOpacity(Alpha);
		Marker.AppliedOpacity = Alpha;
	}
}

void UBGPingDisplayComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	for (const FBGPingMarker& Marker : ActiveMarkers)
	{
		if (IsMarkerStale(Marker, Now))
		{
			RepackMarkers();
			return;
		}
	}

	const FLayout Layout = ReadLayout();
	for (FBGPingMarker& Marker : ActiveMarkers)
	{
		ApplyMarkerTransform(Marker, Now, Layout);
	}

	if (ActiveMarkers.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}
