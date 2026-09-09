#include "FX/BGCardGameplayCueNotify.h"

#include "BGGameplayTags.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Core/BGCharacter.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerController.h"
#include "UI/Presentation/BGPresentationDirector.h"
#include "Debug/BGCardLog.h"
#include "FX/BGCardNiagaraTypes.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	FGameplayTag FindCardId(const FGameplayTagContainer& Tags)
	{
		const FGameplayTag CardIdRoot = FGameplayTag::RequestGameplayTag(TEXT("Card.Id"), false);
		for (const FGameplayTag& Tag : Tags)
		{
			if (CardIdRoot.IsValid() && Tag.MatchesTag(CardIdRoot) && Tag != CardIdRoot)
			{
				return Tag;
			}
		}
		return FGameplayTag();
	}

	const TSoftObjectPtr<UNiagaraSystem>* SelectCardSpecificEffect(
		const FBGCardFXRow& Row, bool bPrimary, const FGameplayTagContainer& Tags)
	{
		if (bPrimary)
		{
			return &Row.PrimaryEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::CriticalSuccess.GetTag()))
		{
			return &Row.CriticalSuccessEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::Success.GetTag()))
		{
			return &Row.SuccessEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::Failure.GetTag()))
		{
			return &Row.FailureEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::CriticalFailure.GetTag()))
		{
			return &Row.CriticalFailureEffect;
		}
		return nullptr;
	}

	const TSoftObjectPtr<UNiagaraSystem>* SelectCharacterAttackEffect(
		const FBGCharacterAttackFX& AttackFX, bool bPrimary, const FGameplayTagContainer& Tags)
	{
		if (bPrimary)
		{
			return &AttackFX.AttackEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::CriticalSuccess.GetTag()))
		{
			return &AttackFX.DamageCriticalEffect;
		}
		if (Tags.HasTagExact(BG::Resolution::Success.GetTag()))
		{
			return &AttackFX.DamageSuccessEffect;
		}
		return nullptr;
	}
}

//게임플레이 큐 생성자
ABGCardGameplayCueNotify::ABGCardGameplayCueNotify()
{
	bAutoDestroyOnRemove = false;
	bAutoAttachToOwner = false;
	bUniqueInstancePerInstigator = true;
	PrimaryActorTick.bCanEverTick = false;
}

//활성화 호출시에 에셋 할당되어 있으면 애님 몽타주 및 사운드 재생
bool ABGCardGameplayCueNotify::OnActive_Implementation(AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	Super::OnActive_Implementation(MyTarget, Parameters);

	if (GetNetMode() != NM_DedicatedServer)
	{
		// Anim Montage 시작 위치. 구체 자산/분기는 GameplayCue Blueprint에서 구현한다.
		K2_PlayCardAnimation(MyTarget, Parameters);

		// Sound 시작 위치. one-shot 또는 loop 선택도 GameplayCue Blueprint에서 구현한다.
		K2_PlayCardSound(MyTarget, Parameters);
	}
	return true;
}

//활성화 되어 있으면 나이아가라 이펙트 스폰
bool ABGCardGameplayCueNotify::WhileActive_Implementation(AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	Super::WhileActive_Implementation(MyTarget, Parameters);
	SpawnCardNiagara(MyTarget, Parameters);
	return true;
}

//종료 호출시 애님 몽타주 및 사운드 중단, 이펙트 종료 딜레이 후 FinishCueRemoval()로 최종적으로 종료 및 비활성화
bool ABGCardGameplayCueNotify::OnRemove_Implementation(AActor* MyTarget,
	const FGameplayCueParameters& Parameters)
{
	if (GetNetMode() != NM_DedicatedServer)
	{
		// Anim Montage/Sound 종료 위치. 정지 또는 페이드아웃은 Blueprint에서 구현한다.
		K2_StopCardAnimationAndSound(MyTarget, Parameters);
	}

	if (SpawnedEffect && !EndBoolParameter.IsNone())
	{
		SpawnedEffect->SetVariableBool(EndBoolParameter, true);
	}

	if (SpawnedEffect && !EndBoolParameter.IsNone() && EndDelaySeconds > 0.f && GetWorld())
	{
		GetWorldTimerManager().SetTimer(EndTimer, this,
			&ABGCardGameplayCueNotify::FinishCueRemoval, EndDelaySeconds, false);
	}
	else
	{
		FinishCueRemoval();
	}
	return true;
}

//	월드 연출이 끝났음을 연출 큐에 알린다 (시네마틱 없는 카드 전용).
//	⚠️ 시네마틱 카드는 단계마다 Cue 가 붙었다 떨어져 여기서 알리면 중간에 꺼진다 —
//	   그쪽 끝은 ABGPlayerController::HandleCinematicStageChangedForFX 가 맡는다.
void ABGCardGameplayCueNotify::NotifyHandPlayFXFinished()
{
	//	연출은 화면마다 따로 돈다 — 찾는 것은 이 클라이언트의 디렉터다.
	const UWorld* World = GetWorld();
	ABGPlayerController* PC = World
		? Cast<ABGPlayerController>(World->GetFirstPlayerController())
		: nullptr;

	if (UBGPresentationDirector* Director = PC ? PC->GetPresentationDirector() : nullptr)
	{
		Director->NotifyWorldFXFinished();
	}
}

//나이아가라 VFX 스폰
void ABGCardGameplayCueNotify::SpawnCardNiagara(AActor* MyTarget,
	const FGameplayCueParameters& Parameters, bool bAllowSpawnDelay)
{
	if (GetNetMode() == NM_DedicatedServer || SpawnedEffect || !MyTarget)
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	const UBGDataSubsystem* Data = GI ? GI->GetSubsystem<UBGDataSubsystem>() : nullptr;
	const FGameplayTag CardId = FindCardId(Parameters.AggregatedSourceTags);
	const FBGCardFXRow* Row = Data ? Data->GetCardFXRow(CardId) : nullptr;
	if (!Row)
	{
		return;
	}

	const FGameplayTag CueTag = Parameters.OriginalTag.IsValid()
		? Parameters.OriginalTag
		: Parameters.MatchedTagName;
	const bool bPrimary = CueTag.MatchesTagExact(BG::GameplayCue::Card::Primary.GetTag());
	const float SpawnDelaySeconds = bPrimary
		? Row->PrimarySpawnDelaySeconds
		: Row->SecondarySpawnDelaySeconds;

	if (bAllowSpawnDelay && SpawnDelaySeconds > 0.f && GetWorld())
	{
		// WhileActive가 다시 통지되어도 같은 Cue의 스폰 예약은 하나만 유지한다.
		if (!GetWorldTimerManager().IsTimerActive(SpawnTimer))
		{
			const TWeakObjectPtr<AActor> WeakTarget = MyTarget;
			const FGameplayCueParameters DelayedParameters = Parameters;
			GetWorldTimerManager().SetTimer(SpawnTimer,
				FTimerDelegate::CreateWeakLambda(this, [this, WeakTarget, DelayedParameters]()
				{
					if (AActor* Target = WeakTarget.Get())
					{
						SpawnCardNiagara(Target, DelayedParameters, false);
					}
				}),
				SpawnDelaySeconds, false);

			UE_LOG(LogBGCard, Verbose, TEXT("[CardCue] Niagara 스폰 예약 — %.2fs / %s"),
				SpawnDelaySeconds, *MyTarget->GetName());
		}
		return;
	}

	const TSoftObjectPtr<UNiagaraSystem>* Effect = nullptr;
	if (Row->FXSource == EBGCardFXSource::CharacterAttack)
	{
		const ABGCharacter* SourceCharacter = Cast<ABGCharacter>(Parameters.GetInstigator());
		const FBGCharacterRow* CharacterRow = SourceCharacter
			? Data->GetCharacterRow(SourceCharacter->GetJobTag())
			: nullptr;
		if (CharacterRow)
		{
			Effect = SelectCharacterAttackEffect(CharacterRow->AttackFX, bPrimary,
				Parameters.AggregatedSourceTags);
		}
	}
	else
	{
		Effect = SelectCardSpecificEffect(*Row, bPrimary, Parameters.AggregatedSourceTags);
	}

	if (!Effect || Effect->IsNull())
	{
		return;
	}

	UNiagaraSystem* System = Effect->LoadSynchronous();
	USceneComponent* AttachTo = Parameters.TargetAttachComponent.Get();
	if (!AttachTo)
	{
		AttachTo = MyTarget->GetRootComponent();
	}

	FName AttachPoint = NAME_None;
	if (!Row->AttachSocket.IsNone())
	{
		if (const ABGCharacter* TargetCharacter = Cast<ABGCharacter>(MyTarget))
		{
			if (USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh())
			{
				AttachTo = Mesh;
			}
		}
		AttachPoint = Row->AttachSocket;
	}
	if (!System || !AttachTo)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[CardCue] Niagara 또는 부착 대상 없음 — %s"),
			*Effect->ToString());
		return;
	}

	FVector SpawnOffset = Row->LocationOffset;
	SpawnOffset.Z += Data->GetGlobalFXSpawnZOffset();
	SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, AttachTo, AttachPoint, SpawnOffset, Row->RotationOffset,
		EAttachLocation::KeepRelativeOffset, false);
	if (SpawnedEffect)
	{
		SpawnedEffect->SetRelativeScale3D(FVector(Row->Scale));
		EndBoolParameter = Row->EndBoolParameter;
		EndDelaySeconds = Row->EndDelaySeconds;
		UE_LOG(LogBGCard, Verbose, TEXT("[CardCue] 재생 — %s / %s"),
			*System->GetName(), *MyTarget->GetName());
	}
}

//이펙트 종료후 비활성화
void ABGCardGameplayCueNotify::FinishCueRemoval()
{
	//	⚠️ 시네마틱 중이면 알리지 않는다. 단계가 넘어가느라 Cue 가 잠깐 떨어진 것뿐이라,
	//	   여기서 알리면 연출 중간에 HUD 만 먼저 풀린다.
	const ABGGameState* GS = GetWorld() ? GetWorld()->GetGameState<ABGGameState>() : nullptr;
	if (!GS || !GS->IsCinematicPlaying())
	{
		NotifyHandPlayFXFinished();
	}

	GetWorldTimerManager().ClearTimer(SpawnTimer);
	GetWorldTimerManager().ClearTimer(EndTimer);
	if (SpawnedEffect)
	{
		SpawnedEffect->SetAutoDestroy(true);
		SpawnedEffect->Deactivate();
		SpawnedEffect = nullptr;
	}
	EndBoolParameter = NAME_None;
	EndDelaySeconds = 0.f;
	K2_EndGameplayCue();
}

//종료시 타이머 초기화 및 이펙트 비활성화
void ABGCardGameplayCueNotify::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	//	⚠️ 안전망. Cue 액터가 통째로 사라지면(레벨 전환) 끝 신호를 낼 사람이 없어진다.
	NotifyHandPlayFXFinished();

	if (GetWorld())
	{
		GetWorldTimerManager().ClearTimer(SpawnTimer);
		GetWorldTimerManager().ClearTimer(EndTimer);
	}
	if (SpawnedEffect)
	{
		SpawnedEffect->DestroyComponent();
		SpawnedEffect = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}
