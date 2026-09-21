#include "Ping/BGPingComponent.h"

#include "BGGameplayTags.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "CollisionQueryParams.h"
#include "Core/BGCharacter.h"
#include "Core/BGPlayerState.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "UI/Target/BGTargetHighlight.h"
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
	SetLookInputBlocked(false);
	SetTargetHighlight(INDEX_NONE, false);

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
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] 거부: 판 진행 중이 아님(%s)"), *Phase.ToString());
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
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] 거부: 대상 좌석 %d 무효·사망"), TargetSeat);
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
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] 거부: 서버 쿨다운(좌석 %d)"), SenderSeat);
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
//
//	클릭으로 켜고 끈다. 휠 버튼을 한 번 누르면 커서 아래 캐릭터를 대상으로 휠이 열리고,
//	마우스를 움직여 칸을 고른 뒤 한 번 더 누르면 그 칸으로 나간다.

void UBGPingComponent::BindInput(UInputComponent* InInputComponent)
{
	if (!InInputComponent || !PingKey.IsValid())
	{
		return;
	}

	InInputComponent->BindKey(PingKey, IE_Pressed, this, &UBGPingComponent::HandlePingPressed);
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

	//	이미 열려 있으면 이번 클릭은 확정이다.
	if (bWheelOpen)
	{
		ConfirmWheelSelection();
		return;
	}

	FVector2D MousePos;
	if (!GetMouseScreenPos(MousePos))
	{
		return;
	}

	PressTargetSeat = FindSeatUnderCursor();
	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 클릭 — 커서 아래 좌석 %d"), PressTargetSeat);

	//	핑은 캐릭터 위에서만 연다 — 빈 곳을 누르면 아무 일도 없다.
	if (PressTargetSeat == INDEX_NONE)
	{
		return;
	}

	PressScreenPos = MousePos;

	SetLookInputBlocked(true);
	SetTargetHighlight(PressTargetSeat, true);
	OpenWheel();
	SetComponentTickEnabled(true);
}

void UBGPingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bWheelOpen)
	{
		SetComponentTickEnabled(false);
		return;
	}

	FVector2D MousePos;
	if (!GetMouseScreenPos(MousePos) || !WheelWidget)
	{
		return;
	}

	//	커서가 중심에서 어느 쪽으로 벗어났는지가 곧 선택이다.
	WheelWidget->SetHighlighted(DirectionFromDrag(MousePos - PressScreenPos));
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

int32 UBGPingComponent::FindSeatUnderCursor() const
{
	const ABGPlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return INDEX_NONE;
	}

	//	이름표 쪽 판정을 먼저 믿는다 — 같은 화면을 보고 있으므로 결과가 일치해야 한다.
	if (const UBGSeatInfoDisplayComponent* SeatInfo = PC->FindComponentByClass<UBGSeatInfoDisplayComponent>())
	{
		const int32 Hovered = SeatInfo->GetHoveredSeat();
		if (Hovered != INDEX_NONE)
		{
			return Hovered;
		}
	}

	//	멀리 앉은 사람은 화면에서 작아 얇은 선 판정이 자주 빗나간다. 굵게 한 번 더 쓸어 본다.
	FVector Origin;
	FVector Direction;
	if (!PC->DeprojectMousePositionToWorld(Origin, Direction))
	{
		return INDEX_NONE;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BGPingSweep), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(PC->GetPawn());

	FHitResult Hit;
	const UWorld* World = GetWorld();
	if (!World || !World->SweepSingleByChannel(Hit, Origin, Origin + Direction * CursorTraceDistance,
			FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(CursorSweepRadius), Params))
	{
		return INDEX_NONE;
	}

	const ABGCharacter* Character = Cast<ABGCharacter>(Hit.GetActor());
	const ABGPlayerState* PS = Character ? Character->GetPlayerState<ABGPlayerState>() : nullptr;
	return PS ? PS->GetSeatIndex() : INDEX_NONE;
}

void UBGPingComponent::SetTargetHighlight(int32 Seat, bool bOn)
{
	if (!bOn)
	{
		Seat = HighlightedSeat;
	}

	const ABGGameState* GS = GetBGGameState();
	if (!GS || Seat == INDEX_NONE)
	{
		HighlightedSeat = INDEX_NONE;
		return;
	}

	ABGPlayerState* PS = const_cast<ABGGameState*>(GS)->GetPlayerStateBySeat(Seat);
	ABGCharacter* Character = PS ? Cast<ABGCharacter>(PS->GetPawn()) : nullptr;
	if (!Character)
	{
		HighlightedSeat = INDEX_NONE;
		return;
	}

	BGTargetHighlight::Apply(Character,
		bOn ? BGTargetHighlight::EState::Aimed : BGTargetHighlight::EState::None);
	HighlightedSeat = bOn ? Seat : INDEX_NONE;
}

void UBGPingComponent::SetLookInputBlocked(bool bBlocked)
{
	if (bLookInputBlocked == bBlocked)
	{
		return;
	}

	ABGPlayerController* PC = GetOwningPC();
	if (!PC)
	{
		return;
	}

	//	SetIgnoreLookInput 은 누적 카운터다 — 켠 만큼만 정확히 끈다.
	PC->SetIgnoreLookInput(bBlocked);
	bLookInputBlocked = bBlocked;
}

void UBGPingComponent::CloseWheel()
{
	bWheelOpen = false;
	SetComponentTickEnabled(false);
	SetLookInputBlocked(false);
	SetTargetHighlight(INDEX_NONE, false);

	if (WheelWidget)
	{
		WheelWidget->Close();
	}
}

void UBGPingComponent::CancelWheel()
{
	if (!bWheelOpen)
	{
		return;
	}

	UE_LOG(LogBGPing, Log, TEXT("[BGPing] 휠 취소"));
	CloseWheel();
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
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] 무시: 클라 쿨다운"));
		return false;
	}

	RecentSendTimes.RemoveAll([this, Now](double Time) { return Now - Time > RateWindowSec; });
	if (RecentSendTimes.Num() >= MaxPingsPerWindow)
	{
		UE_LOG(LogBGPing, Log, TEXT("[BGPing] 무시: %.0f초 %d회 상한"), RateWindowSec, MaxPingsPerWindow);
		return false;
	}

	const uint64 Key = (static_cast<uint64>(static_cast<uint32>(TargetSeat)) << 32)
		^ static_cast<uint64>(GetTypeHash(PingTag));
	if (const double* Last = LastSendByTargetAndTag.Find(Key))
	{
		if (Now - *Last < DuplicateIgnoreSec)
		{
			UE_LOG(LogBGPing, Log, TEXT("[BGPing] 무시: 같은 대상·같은 종류 중복"));
			return false;
		}
	}

	LastClientSendTime = Now;
	RecentSendTimes.Add(Now);
	LastSendByTargetAndTag.Add(Key, Now);
	return true;
}

void UBGPingComponent::ConfirmWheelSelection()
{
	FVector2D MousePos = PressScreenPos;
	GetMouseScreenPos(MousePos);

	const EBGPingWheelDirection Direction = DirectionFromDrag(MousePos - PressScreenPos);
	const int32 TargetSeat = PressTargetSeat;

	CloseWheel();

	const FGameplayTag PingTag = TagFromDirection(Direction);
	if (!PingTag.IsValid())
	{
		return;
	}

	if (!PassesClientFilters(PingTag, TargetSeat))
	{
		return;
	}

	SendPing(PingTag, TargetSeat);
}

void UBGPingComponent::DebugOpenWheel(int32 TargetSeat)
{
	PressTargetSeat = TargetSeat;
	PressScreenPos = FVector2D(640.f, 360.f);
	GetMouseScreenPos(PressScreenPos);
	SetTargetHighlight(TargetSeat, true);
	OpenWheel();
	SetComponentTickEnabled(true);
}
