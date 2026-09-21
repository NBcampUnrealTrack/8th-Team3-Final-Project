#include "Ping/BGPingComponent.h"

#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "Core/BGPlayerState.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "UI/Target/BGSeatInfoDisplayComponent.h"
#include "UI/Widget/BGPingWheelWidget.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"

UBGPingComponent::UBGPingComponent()
{
	//	누르고 있는 동안만 돈다 — 방향 추적이 끝나면 스스로 끈다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UBGPingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PingTable.IsNull())
	{
		LoadedPingTable = PingTable.LoadSynchronous();
	}

	PreloadIcons();
}

void UBGPingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IconPreloadHandle.IsValid())
	{
		IconPreloadHandle->ReleaseHandle();
		IconPreloadHandle.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UBGPingComponent::PreloadIcons()
{
	TArray<FSoftObjectPath> Paths;
	if (LoadedPingTable)
	{
		LoadedPingTable->ForeachRow<FBGPingDef>(TEXT("BGPing.Preload"),
			[&Paths](const FName&, const FBGPingDef& Def)
			{
				if (!Def.Icon.IsNull())
				{
					Paths.Add(Def.Icon.ToSoftObjectPath());
				}
			});
	}

	if (Paths.Num() == 0)
	{
		return;
	}

	IconPreloadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
}

ABGPlayerController* UBGPingComponent::GetOwningPC() const
{
	return Cast<ABGPlayerController>(GetOwner());
}

ABGGameState* UBGPingComponent::GetBGGameState() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetGameState<ABGGameState>() : nullptr;
}

const FBGPingDef* UBGPingComponent::FindDef(FGameplayTag PingTag) const
{
	if (LoadedPingTable)
	{
		static const FString Ctx(TEXT("BGPing.FindDef"));
		TArray<FBGPingDef*> Rows;
		LoadedPingTable->GetAllRows<FBGPingDef>(Ctx, Rows);
		for (const FBGPingDef* Row : Rows)
		{
			if (Row && Row->PingTag == PingTag)
			{
				return Row;
			}
		}
	}

	return BGPing::FindDefaultDef(PingTag);
}

void UBGPingComponent::SendPing(FGameplayTag PingTag, int32 TargetSeat)
{
	ABGPlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return;
	}

	const FBGPingDef* Def = FindDef(PingTag);
	if (!Def)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 알 수 없는 핑 태그 %s — 무시"), *PingTag.ToString());
		return;
	}

	if (Def->bNeedsTarget && TargetSeat == INDEX_NONE)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] %s 는 대상이 필요하다 — 발신 안 함"), *PingTag.ToString());
		return;
	}

	PC->ServerSendPing(PingTag, TargetSeat);
}

bool UBGPingComponent::ServerValidate(FGameplayTag PingTag, int32 SenderSeat, int32 TargetSeat) const
{
	ABGGameState* GS = GetBGGameState();
	if (!ensureMsgf(GS, TEXT("[BGPing] GameState 가 없다")))
	{
		return false;
	}

	const FGameplayTag Phase = GS->GetPhaseTag();
	if (!Phase.IsValid() || Phase == BG::Phase::Lobby.GetTag() || Phase == BG::Phase::GameOver.GetTag())
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 거부: 판 진행 중이 아님(%s)"), *Phase.ToString());
		return false;
	}

	if (SenderSeat == INDEX_NONE || !GS->IsSeatAlive(SenderSeat))
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 거부: 발신자 좌석 %d 무효·사망"), SenderSeat);
		return false;
	}

	const FBGPingDef* Def = FindDef(PingTag);
	if (!Def)
	{
		UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 거부: 정의에 없는 태그 %s"), *PingTag.ToString());
		return false;
	}

	if (Def->bNeedsTarget && (TargetSeat == INDEX_NONE || !GS->IsSeatAlive(TargetSeat)))
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 거부: 대상 좌석 %d 무효·사망"), TargetSeat);
		return false;
	}

	if (TargetSeat != INDEX_NONE && !Def->bNeedsTarget && !GS->IsSeatAlive(TargetSeat))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastServerSendTime < ServerCooldownSec)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 거부: 서버 쿨다운(좌석 %d)"), SenderSeat);
		return false;
	}

	return true;
}

void UBGPingComponent::HandleServerSendPing(FGameplayTag PingTag, int32 TargetSeat)
{
	ABGPlayerController* PC = GetOwningPC();
	ABGGameState* GS = GetBGGameState();
	if (!PC || !GS)
	{
		return;
	}

	const ABGPlayerState* PS = PC->GetPlayerState<ABGPlayerState>();
	const int32 SenderSeat = PS ? PS->GetSeatIndex() : INDEX_NONE;

	if (!ServerValidate(PingTag, SenderSeat, TargetSeat))
	{
		return;
	}

	const UWorld* World = GetWorld();
	LastServerSendTime = World ? World->GetTimeSeconds() : 0.0;

	FBGPingEvent Event;
	Event.SenderSeat = SenderSeat;
	Event.TargetSeat = TargetSeat;
	Event.PingTag = PingTag;
	Event.ServerTime = GS->GetServerWorldTimeSeconds();

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 발신 %d → %d : %s"),
		SenderSeat, TargetSeat, *PingTag.ToString());

	GS->Multicast_Ping(Event);
}

// ── 입력·휠 ─────────────────────────────────────────────────────

void UBGPingComponent::BindInput(UInputComponent* InInputComponent)
{
	if (!InInputComponent || !PingKey.IsValid())
	{
		return;
	}

	InInputComponent->BindKey(PingKey, IE_Pressed, this, &UBGPingComponent::HandlePingPressed);
	InInputComponent->BindKey(PingKey, IE_Released, this, &UBGPingComponent::HandlePingReleased);
}

bool UBGPingComponent::GetMouseScreenPos(FVector2D& OutPos) const
{
	const ABGPlayerController* PC = GetOwningPC();
	float X = 0.f, Y = 0.f;
	if (!PC || !PC->GetMousePosition(X, Y))
	{
		return false;
	}

	OutPos = FVector2D(X, Y);
	return true;
}

void UBGPingComponent::HandlePingPressed()
{
	const ABGPlayerController* PC = GetOwningPC();
	if (!PC || !PC->IsLocalController())
	{
		return;
	}

	FVector2D MousePos;
	if (!GetMouseScreenPos(MousePos))
	{
		return;
	}

	const UBGSeatInfoDisplayComponent* SeatInfo = PC->FindComponentByClass<UBGSeatInfoDisplayComponent>();
	PressTargetSeat = SeatInfo ? SeatInfo->GetHoveredSeat() : INDEX_NONE;
	PressScreenPos = MousePos;
	PressTime = FPlatformTime::Seconds();
	bPressed = true;
	bWheelOpen = false;

	SetComponentTickEnabled(true);
}

void UBGPingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPressed)
	{
		SetComponentTickEnabled(false);
		return;
	}

	FVector2D MousePos;
	if (!GetMouseScreenPos(MousePos))
	{
		return;
	}

	const FVector2D Delta = MousePos - PressScreenPos;
	if (!bWheelOpen)
	{
		const bool bFarEnough = Delta.Size() >= WheelOpenPx;
		const bool bHeldLongEnough = (FPlatformTime::Seconds() - PressTime) >= WheelOpenDelaySec;
		if (bFarEnough || bHeldLongEnough)
		{
			OpenWheel();
		}
	}

	if (bWheelOpen && WheelWidget)
	{
		WheelWidget->SetHighlighted(DirectionFromDrag(Delta));
	}
}

void UBGPingComponent::OpenWheel()
{
	bWheelOpen = true;

	if (!WheelWidgetClass)
	{
		return;
	}

	ABGPlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return;
	}

	if (!WheelWidget)
	{
		WheelWidget = CreateWidget<UBGPingWheelWidget>(PC, WheelWidgetClass);
		if (!WheelWidget)
		{
			return;
		}
	}

	if (!WheelWidget->IsInViewport())
	{
		WheelWidget->AddToViewport(50);
	}

	for (const FBGPingDef& Def : BGPing::GetDefaultDefs())
	{
		const FBGPingDef* Row = FindDef(Def.PingTag);
		const FBGPingDef& Use = Row ? *Row : Def;
		WheelWidget->SetSlotIcon(Use.WheelDirection, Use.Icon.Get(), Use.IconColor);
	}

	WheelWidget->Open(PressScreenPos, PressTargetSeat != INDEX_NONE);
}

void UBGPingComponent::DebugOpenWheel(int32 TargetSeat)
{
	PressTargetSeat = TargetSeat;
	PressScreenPos = FVector2D(640.f, 360.f);
	GetMouseScreenPos(PressScreenPos);
	OpenWheel();
}

void UBGPingComponent::CancelWheel()
{
	if (!bPressed && !bWheelOpen)
	{
		return;
	}

	bPressed = false;
	bWheelOpen = false;
	SetComponentTickEnabled(false);

	if (WheelWidget)
	{
		WheelWidget->Close();
	}
}

EBGPingWheelDirection UBGPingComponent::DirectionFromDrag(const FVector2D& Delta) const
{
	if (Delta.Size() < DeadZonePx)
	{
		return EBGPingWheelDirection::Center;
	}

	//	화면 Y 는 아래로 자란다. 각 방향 90도씩 4분할.
	if (FMath::Abs(Delta.X) >= FMath::Abs(Delta.Y))
	{
		return Delta.X >= 0.f ? EBGPingWheelDirection::Right : EBGPingWheelDirection::Left;
	}

	return Delta.Y < 0.f ? EBGPingWheelDirection::Up : EBGPingWheelDirection::Down;
}

FGameplayTag UBGPingComponent::TagFromDirection(EBGPingWheelDirection Direction) const
{
	for (const FBGPingDef& Def : BGPing::GetDefaultDefs())
	{
		if (Def.WheelDirection == Direction)
		{
			return Def.PingTag;
		}
	}

	return FGameplayTag();
}

bool UBGPingComponent::PassesClientFilters(FGameplayTag PingTag, int32 TargetSeat)
{
	const double Now = FPlatformTime::Seconds();

	if (Now - LastClientSendTime < ClientCooldownSec)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 클라 쿨다운 — 무시"));
		return false;
	}

	RecentSendTimes.RemoveAll([this, Now](double Time) { return Now - Time > RateWindowSec; });
	if (RecentSendTimes.Num() >= MaxPingsPerWindow)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] %.0f초 %d회 상한 — 무시"), RateWindowSec, MaxPingsPerWindow);
		return false;
	}

	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(TargetSeat)) << 32)
		^ static_cast<uint64>(GetTypeHash(PingTag));
	if (const double* Last = LastSendByTargetAndTag.Find(Key))
	{
		if (Now - *Last < DuplicateIgnoreSec)
		{
			UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] 같은 대상·같은 종류 중복 — 무시"));
			return false;
		}
	}

	LastClientSendTime = Now;
	RecentSendTimes.Add(Now);
	LastSendByTargetAndTag.Add(Key, Now);
	return true;
}

void UBGPingComponent::HandlePingReleased()
{
	if (!bPressed)
	{
		return;
	}

	bPressed = false;
	SetComponentTickEnabled(false);

	FVector2D MousePos = PressScreenPos;
	GetMouseScreenPos(MousePos);

	const EBGPingWheelDirection Direction = DirectionFromDrag(MousePos - PressScreenPos);

	if (WheelWidget)
	{
		WheelWidget->Close();
	}
	bWheelOpen = false;

	const FGameplayTag PingTag = TagFromDirection(Direction);
	if (!PingTag.IsValid())
	{
		return;
	}

	const FBGPingDef* Def = FindDef(PingTag);
	if (Def && Def->bNeedsTarget && PressTargetSeat == INDEX_NONE)
	{
		UE_LOG(LogBGPing, Verbose, TEXT("[BGPing] %s 는 대상이 필요하다 — 발신 안 함"), *PingTag.ToString());
		return;
	}

	const int32 TargetSeat = PressTargetSeat;
	if (!PassesClientFilters(PingTag, TargetSeat))
	{
		return;
	}

	SendPing(PingTag, TargetSeat);
}
