#include "FX/BGStatusFXComponent.h"

#include "FX/BGStatusFXTypes.h"
#include "Card/BGCardType.h"
#include "Card/BGDataSubsystem.h"
#include "Core/BGPlayerState.h"
#include "Debug/BGCardLog.h"

#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
	//	PlayerState 를 기다리는 간격(초). 붙는 즉시 멈추므로 짧아도 몇 번 돌지 않는다.
	constexpr float PlayerStateWaitInterval = 0.25f;
}

UBGStatusFXComponent::UBGStatusFXComponent()
{
	//	통지로만 움직인다. 매 프레임 할 일이 없다.
	PrimaryComponentTick.bCanEverTick = false;
}

void UBGStatusFXComponent::BeginPlay()
{
	Super::BeginPlay();

	//	화면에만 쓰이는 것이라 그릴 화면이 없으면 아무것도 하지 않는다.
	if (IsRunningDedicatedServer())
	{
		return;
	}

	PublicInfoHandle = ABGPlayerState::OnAnySeatPublicInfoChanged.AddUObject(
		this, &UBGStatusFXComponent::HandleSeatPublicInfoChanged);

	//	즉시 한 번 보고, 아직이면 붙을 때까지만 기다린다.
	PollPlayerState();
}

void UBGStatusFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PublicInfoHandle.IsValid())
	{
		ABGPlayerState::OnAnySeatPublicInfoChanged.Remove(PublicInfoHandle);
		PublicInfoHandle.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerStateWaitTimer);
	}

	//	여기서 정리하지 않으면 폰이 파괴돼도 이펙트만 월드에 남는다
	//	(부모가 사라지므로 원점에서 계속 터진다).
	for (const TPair<FName, TObjectPtr<UNiagaraComponent>>& Pair : ActiveFX)
	{
		if (UNiagaraComponent* FX = Pair.Value)
		{
			FX->DestroyComponent();
		}
	}
	ActiveFX.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBGStatusFXComponent::PollPlayerState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (GetOwnerPlayerState())
	{
		World->GetTimerManager().ClearTimer(PlayerStateWaitTimer);
		RefreshStatusFX();
		return;
	}

	if (!PlayerStateWaitTimer.IsValid())
	{
		World->GetTimerManager().SetTimer(PlayerStateWaitTimer, this,
			&UBGStatusFXComponent::PollPlayerState, PlayerStateWaitInterval, /*bLoop=*/true);
	}
}

void UBGStatusFXComponent::HandleSeatPublicInfoChanged(int32 ChangedSeat)
{
	const ABGPlayerState* PS = GetOwnerPlayerState();

	//	내 좌석을 아직 모르면 걸러낼 근거가 없다. 재조정은 몇 번 불려도 결과가 같으므로
	//	그냥 돈다 — 거르려다 놓치는 쪽이 훨씬 나쁘다(화면만 조용히 낡는다).
	if (!PS || PS->GetSeatIndex() == INDEX_NONE || PS->GetSeatIndex() == ChangedSeat)
	{
		RefreshStatusFX();
	}
}

void UBGStatusFXComponent::BuildDesiredKeys(TSet<FName>& OutKeys) const
{
	OutKeys.Reset();

	const ABGPlayerState* PS = GetOwnerPlayerState();
	if (!PS)
	{
		return;
	}

	//	화면상 죽었으면 전부 끈다. bIsDead(규칙상 사망)가 아니라 이쪽을 보는 이유는
	//	그 둘이 일부러 갈라져 있어서다 — bIsDead 로 판단하면 죽는 일격을 맞는 순간
	//	오라가 먼저 꺼지고 그 뒤에 공격·피격 연출이 재생된다.
	//
	//	규칙 쪽 정리(ClearPendingStatuses / TakeAllCards)는 사망 연출이 다 끝난 뒤
	//	ProcessSeatDeath 에서 일어나므로, 이 검사가 없으면 쓰러지고 역할을 공개하는
	//	내내 장비 오라가 켜져 있다.
	if (PS->IsDeathShown())
	{
		return;
	}

	for (const FBGCardInstance& Card : PS->GetEquipment())
	{
		OutKeys.Add(Card.CardId);              //	DT_BGCard 의 Row 이름 = "Card.Id.*"
	}

	for (const FBGPendingStatus& Status : PS->GetPendingStatuses())
	{
		if (Status.Tag.IsValid())
		{
			OutKeys.Add(Status.Tag.GetTagName());   //	"Status.*"
		}
	}
}

void UBGStatusFXComponent::RefreshStatusFX()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	TSet<FName> Desired;
	BuildDesiredKeys(Desired);

	// ── 1. 없어진 것 정리 ──
	for (auto It = ActiveFX.CreateIterator(); It; ++It)
	{
		if (Desired.Contains(It->Key))
		{
			continue;
		}

		if (UNiagaraComponent* FX = It->Value)
		{
			//	바로 파괴하면 남아 있던 파티클까지 한 프레임에 사라져 뚝 끊긴다.
			//	방출만 멈추고, 남은 것이 다 사라지면 스스로 정리되게 한다.
			FX->SetAutoDestroy(true);
			FX->Deactivate();
		}

		UE_LOG(LogBGCard, Verbose, TEXT("[StatusFX] %s 제거"), *It->Key.ToString());
		It.RemoveCurrent();
	}

	// ── 2. 새로 생긴 것 부착 ──
	for (const FName& Key : Desired)
	{
		if (ActiveFX.Contains(Key))
		{
			continue;
		}

		if (UNiagaraComponent* FX = SpawnFXFor(Key))
		{
			ActiveFX.Add(Key, FX);
			UE_LOG(LogBGCard, Verbose, TEXT("[StatusFX] %s 부착"), *Key.ToString());
		}
	}
}

UNiagaraComponent* UBGStatusFXComponent::SpawnFXFor(FName Key)
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	if (!Data)
	{
		return nullptr;
	}

	const FBGStatusFXRow* Row = Data->GetStatusFXRow(Key);
	if (!Row)
	{
		return nullptr;   //	표에 없다 = 이펙트 없음. 정상이다.
	}

	//	Row 는 있는데 Effect 가 비어 있는 것도 정상이다 — 아직 이펙트를 안 만든 항목을
	//	이름만 올려 둘 수 있게 한 것이다.
	if (Row->Effect.IsNull())
	{
		return nullptr;
	}

	//	동기 로드다. 장착·상태 부여 순간에만 일어나고 이펙트 하나는 가벼우므로 감수한다.
	//	여기가 눈에 띄게 튀면 판 시작 때 미리 로드하는 쪽으로 옮길 것.
	UNiagaraSystem* System = Row->Effect.LoadSynchronous();
	if (!System)
	{
		UE_LOG(LogBGCard, Warning,
			TEXT("[StatusFX] %s 의 이펙트를 로드하지 못했습니다: %s"),
			*Key.ToString(), *Row->Effect.ToString());
		return nullptr;
	}

	//	캐릭터면 메쉬에, 아니면 루트에 붙인다. 메쉬에 붙여야 소켓을 쓸 수 있고
	//	직업 메쉬가 어떤 자세를 취하든 따라간다.
	USceneComponent* AttachTo = nullptr;
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		AttachTo = OwnerCharacter->GetMesh();
	}
	if (!AttachTo && GetOwner())
	{
		AttachTo = GetOwner()->GetRootComponent();
	}
	if (!AttachTo)
	{
		return nullptr;
	}

	//	bAutoDestroy=false — 이 컴포넌트가 수명을 쥔다. true 로 두면 루프가 아닌 이펙트가
	//	제 스스로 사라져 ActiveFX 에 죽은 포인터가 남고, 그 키는 영영 다시 붙지 않는다.
	UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, AttachTo, Row->AttachSocket,
		Row->LocationOffset, FRotator::ZeroRotator,
		EAttachLocation::SnapToTargetIncludingScale,
		/*bAutoDestroy=*/false);

	if (FX && !FMath::IsNearlyEqual(Row->Scale, 1.f))
	{
		FX->SetRelativeScale3D(FVector(Row->Scale));
	}

	return FX;
}

ABGPlayerState* UBGStatusFXComponent::GetOwnerPlayerState() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return OwnerPawn ? OwnerPawn->GetPlayerState<ABGPlayerState>() : nullptr;
}

TArray<FName> UBGStatusFXComponent::GetActiveFXKeys() const
{
	TArray<FName> Keys;
	ActiveFX.GetKeys(Keys);
	return Keys;
}
