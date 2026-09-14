#include "FX/BGCardGameplayCueNotify.h"

#include "BGGameplayTags.h"
#include "Card/BGDataSubsystem.h"
#include "Card/BGGameDataTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/BGCharacter.h"
#include "UI/Presentation/BGPresentationDirector.h"
#include "Debug/BGCardLog.h"
#include "FX/BGCardNiagaraTypes.h"
#include "FX/BGPresentationDataAsset.h"
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

		// ⚠️ 위 호출 **직후**여야 한다. BP 가 방금 건 몽타주가 지금은 "돌고 있는 몽타주" 다.
		WatchCardAnimation(MyTarget);

		BeginWorldFXTrack();

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

//	이 Cue 가 사는 동안 월드FX 트랙의 표를 쥔다.
//	⚠️ 시네마틱 여부를 여기서 따지지 않는다. 시네마틱 카드는 단계마다 Cue 가 붙었다
//	   떨어지지만, 그동안 시네마틱 트랙이 따로 표를 쥐고 있으므로 연출이 안 풀린다 —
//	   트랙을 나눈 이득이 바로 이 조건문이 사라지는 것이다.
void ABGCardGameplayCueNotify::BeginWorldFXTrack()
{
	WorldFXHold.Begin(UBGPresentationDirector::Get(this), EBGPresentationTrack::WorldFX);
}

void ABGCardGameplayCueNotify::EndWorldFXTrack()
{
	WorldFXHold.End(UBGPresentationDirector::Get(this));
}

void ABGCardGameplayCueNotify::WatchCardAnimation(AActor* MyTarget)
{
	//	⚠️ Cue 액터는 재사용된다(bAutoDestroyOnRemove = false). 앞 카드의 구독과 표가
	//	   남으면 그 몽타주가 끝날 때 이번 연출이 풀린다.
	StopWatchingCardAnimation();

	const ABGCharacter* TargetCharacter = Cast<ABGCharacter>(MyTarget);
	const USkeletalMeshComponent* Mesh = TargetCharacter ? TargetCharacter->GetMesh() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* Montage = AnimInstance ? AnimInstance->GetCurrentActiveMontage() : nullptr;
	if (!Montage)
	{
		return;   // 몽타주 없이 파티클·사운드만 쓰는 카드다. Cue 종료가 곧 끝이다.
	}

	WatchedAnimInstance = AnimInstance;
	WatchedMontage      = Montage;
	AnimInstance->OnMontageEnded.AddDynamic(this, &ABGCardGameplayCueNotify::HandleCardMontageEnded);

	//	연출이 아직 시작 전이어도 된다 — 디렉터가 다음 연출로 물려받는다.
	MontageHold.Begin(UBGPresentationDirector::Get(this), EBGPresentationTrack::Montage);

	UE_LOG(LogBGCard, Verbose, TEXT("[CardCue] 몽타주 종료 대기 — %s / %s"),
		*Montage->GetName(), *MyTarget->GetName());
}

void ABGCardGameplayCueNotify::HandleCardMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	//	같은 메쉬의 다른 몽타주가 끝난 것일 수 있다.
	if (Montage != WatchedMontage.Get())
	{
		return;
	}

	//	⚠️ 중단도 끝으로 친다. 무시하면 다음 카드가 덮어썼을 때 표가 영영 안 놓인다.
	StopWatchingCardAnimation();
}

void ABGCardGameplayCueNotify::StopWatchingCardAnimation()
{
	if (UAnimInstance* AnimInstance = WatchedAnimInstance.Get())
	{
		AnimInstance->OnMontageEnded.RemoveDynamic(this, &ABGCardGameplayCueNotify::HandleCardMontageEnded);
	}

	MontageHold.End(UBGPresentationDirector::Get(this));

	WatchedAnimInstance.Reset();
	WatchedMontage.Reset();
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
	if (!Data)
	{
		return;
	}

	const FGameplayTag CardId = FindCardId(Parameters.AggregatedSourceTags);
	const FGameplayTag CueTag = Parameters.OriginalTag.IsValid()
		? Parameters.OriginalTag
		: Parameters.MatchedTagName;
	const bool bPrimary = CueTag.MatchesTagExact(BG::GameplayCue::Card::Primary.GetTag());
	const bool bCriticalFailure = Parameters.AggregatedSourceTags.HasTagExact(
		BG::Resolution::CriticalFailure.GetTag());
	const bool bFailure = Parameters.AggregatedSourceTags.HasTagExact(
		BG::Resolution::Failure.GetTag());
	const bool bGlobalFailure = bPrimary && (bFailure || bCriticalFailure);
	const UBGPresentationDataAsset* Presentation = Data->GetPresentationData();
	const FBGCardFXRow* Row = Data->GetCardFXRow(CardId);
	if (!Row && !bGlobalFailure)
	{
		return;
	}

	// 실패 계열은 Primary에서 전역 실패 연출 하나만 재생한다. 시네마틱 Secondary까지
	// 통과시키면 카드/직업 피격 FX와 실패 FX가 함께 보일 수 있으므로 여기서 막는다.
	if (!bPrimary && (bFailure || bCriticalFailure))
	{
		return;
	}

	const float SpawnDelaySeconds = bGlobalFailure && Presentation
		? Presentation->FailureSpawnDelaySeconds
		: bPrimary
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
	if (bPrimary && bCriticalFailure)
	{
		Effect = Presentation ? &Presentation->CriticalFailureEffect : nullptr;
	}
	else if (bPrimary && bFailure)
	{
		Effect = Presentation ? &Presentation->FailureEffect : nullptr;
	}
	else if (Row->FXSource == EBGCardFXSource::CharacterAttack)
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
	if (!bGlobalFailure && !Row->AttachSocket.IsNone())
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

	// Row/DataAsset 오프셋은 부착 대상 기준의 개별 로컬 보정으로 적용한다.
	const FVector SpawnOffset = bGlobalFailure && Presentation
		? Presentation->FailureLocationOffset
		: Row->LocationOffset;
	const FRotator SpawnRotation = bGlobalFailure && Presentation
		? Presentation->FailureRotationOffset
		: Row->RotationOffset;
	const float SpawnScale = bGlobalFailure && Presentation
		? Presentation->FailureScale
		: Row->Scale;
	SpawnedEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
		System, AttachTo, AttachPoint, SpawnOffset, SpawnRotation,
		EAttachLocation::KeepRelativeOffset,
		/*bAutoDestroy=*/false,
		/*bAutoActivate=*/false);
	if (SpawnedEffect)
	{
		// 전역 높이 World Z를 적용한다.
		SpawnedEffect->AddWorldOffset(FVector::UpVector * Data->GetGlobalFXSpawnZOffset());
		SpawnedEffect->SetRelativeScale3D(FVector(SpawnScale));
		EndBoolParameter = bGlobalFailure && Presentation
			? Presentation->FailureEndBoolParameter
			: Row->EndBoolParameter;
		EndDelaySeconds = bGlobalFailure && Presentation
			? Presentation->FailureEndDelaySeconds
			: Row->EndDelaySeconds;
		SpawnedEffect->Activate(true);
		UE_LOG(LogBGCard, Verbose, TEXT("[CardCue] 재생 — %s / %s"),
			*System->GetName(), *MyTarget->GetName());
	}
}

//이펙트 종료후 비활성화
void ABGCardGameplayCueNotify::FinishCueRemoval()
{
	//	이 Cue 의 몫은 여기서 끝난다. 시네마틱 여부를 따질 필요가 없다 —
	//	시네마틱이 아직 돌면 그쪽 트랙이 표를 쥐고 있다.
	EndWorldFXTrack();

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
	//	⚠️ 안전망. Cue 액터가 통째로 사라지면(레벨 전환) 표를 놓을 사람이 없어진다.
	StopWatchingCardAnimation();
	EndWorldFXTrack();

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
