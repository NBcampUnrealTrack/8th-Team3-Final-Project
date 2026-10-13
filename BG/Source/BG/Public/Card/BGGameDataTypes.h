// BANG! 데이터 테이블 Row 타입 모음 (Team4Project 의 BaseGameDataTypes 대응)

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "Card/BGCardPresentationTypes.h"
#include "BGGameDataTypes.generated.h"

class UGameplayAbility;
class AActor;
class UGameplayEffect;
class UAnimInstance;
class UNiagaraSystem;
class USkeletalMesh;
class UStaticMesh;
class UTexture2D;

/**
 * 판정 결과 4단계 (GDD §4.1).
 *
 * 각 단계의 폭은 카드마다 다르다 — DT_BGCard 의 OutcomeWeights 가 정하고
 * UBGDiceComponent::RollOutcome 이 굴린다.
 *
 * 눈(Roll)의 방향은 **높은 눈 = 대성공**이다 (§4.2 의 누적 순서가
 * 대실패 → 실패 → 성공 → 대성공 이라서). 구형 ClassifyRoll 과 같은 방향이다.
 */
UENUM(BlueprintType)
enum class EBGDiceOutcome : uint8
{
	CriticalFailure UMETA(DisplayName = "대실패"),
	Failure         UMETA(DisplayName = "실패"),
	Success         UMETA(DisplayName = "성공"),
	CriticalSuccess UMETA(DisplayName = "대성공")
};

/**
 * 4단계 판정의 상대 비율 (GDD §4.2 / §4.3).
 *
 * 합이 100 일 필요는 없다 — 시스템이 정규화한다. 기획은 상대값으로 입력한다.
 * 보정(캐릭터·장비·상태)은 이 비율에 더해진 뒤 정규화된다.
 */
USTRUCT(BlueprintType)
struct FBGOutcomeWeights
{
	GENERATED_BODY()

	//	GDD §4.4: 대실패 비율은 낮게 유지한다. 통제 불능이라는 인상을 주면 공격 자체가 위축된다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 CriticalFailure = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Failure = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Success = 65;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 CriticalSuccess = 10;

	int32 Sum() const { return CriticalFailure + Failure + Success + CriticalSuccess; }

	/** 보정을 더한다. 음수로 내려간 항목은 0 으로 클램프한다 (GDD §4.2). */
	void ApplyBonus(int32 SuccessBonus, int32 CriticalBonus)
	{
		Success         = FMath::Max(0, Success + SuccessBonus);
		CriticalSuccess = FMath::Max(0, CriticalSuccess + CriticalBonus);
	}

	/** 네 등급 모두에 가산 보정. 큰 음수를 주면 그 등급이 사실상 무효화된다(드워프 장갑). */
	void ApplyBonusAll(int32 BonusCF, int32 BonusF, int32 BonusS, int32 BonusCS)
	{
		CriticalFailure = FMath::Max(0, CriticalFailure + BonusCF);
		Failure         = FMath::Max(0, Failure         + BonusF);
		Success         = FMath::Max(0, Success         + BonusS);
		CriticalSuccess = FMath::Max(0, CriticalSuccess + BonusCS);
	}

	/**
	 * 승산 보정. 가산보다 "먼저" 적용해야 한다 —
	 * 큰 음수 가산으로 등급을 무효화하는 장비가 배율에 흔들리면 안 된다.
	 */
	void ApplyMultiplier(float MultCF, float MultS, float MultCS)
	{
		CriticalFailure = FMath::Max(0, FMath::RoundToInt(CriticalFailure * FMath::Max(0.f, MultCF)));
		Success         = FMath::Max(0, FMath::RoundToInt(Success         * FMath::Max(0.f, MultS)));
		CriticalSuccess = FMath::Max(0, FMath::RoundToInt(CriticalSuccess * FMath::Max(0.f, MultCS)));
	}
};

/** 장비 발동 이펙트를 판정 UI 뒤 또는 피격 단계에 재생할지 정한다. */
UENUM(BlueprintType)
enum class EBGEquipmentProcPhase : uint8
{
	AfterResolution,
	Impact
};

/** 서버가 확정한 장비 발동 1건. FXRowKey는 DT_BGStatusFX의 단발 연출 행 이름이다. */
USTRUCT(BlueprintType)
struct FBGEquipmentProcEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	FName FXRowKey;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	TObjectPtr<AActor> Target;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	EBGEquipmentProcPhase Phase = EBGEquipmentProcPhase::AfterResolution;

	/** Impact에서 일반 피격 GameplayCue를 대신할 때만 참. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	bool bReplaceDefaultImpact = false;
};

/**
 * 판정 1회의 결과 — 서버가 굴린 뒤 전원에게 알리는 내용 (GDD §4.1
 * "판정은 서버 권한으로 실행하고 **모든 플레이어에게 동일한 결과를 표시한다**").
 *
 * ⚠️ 멀티캐스트로 나간다. 공개 정보만 담을 것 — 손패 같은 은닉 정보를 넣으면 전원에게 샌다.
 */
USTRUCT(BlueprintType)
struct FBGCardResolution
{
	GENERATED_BODY()

	/** 판정 UI와 결과 FX를 묶는 서버 발급 번호. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	int32 PresentationId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	FGameplayTag CardId;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	EBGDiceOutcome Outcome = EBGDiceOutcome::Failure;

	/** 굴린 눈 0~99. 강제된 판정이면 결과와 어긋나므로 bForced 와 함께 볼 것. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	int32 Roll = INDEX_NONE;

	/** 준비·함정(Status.NextCard.Force*)이 굴림 결과를 덮어썼는가. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	bool bForced = false;

	/** 카드 효과 실행 중 확정된 장비 발동 목록. 여러 장비가 함께 발동해도 보존한다. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	TArray<FBGEquipmentProcEvent> EquipmentProcs;

	/** 카드를 쓴 좌석. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	int32 SeatIndex = INDEX_NONE;
	
	/** 대상 좌석. 대상이 없는 카드(맥주)면 INDEX_NONE. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	int32 TargetSeatIndex = INDEX_NONE;
	
	/**
	 * 판정에 실제로 쓰인 비율(보정 반영 후).
	 *
	 * 클라가 다시 계산하지 않고 이 값을 그대로 쓰게 하려고 함께 보낸다 —
	 * 클라에서 재계산하려면 남의 어트리뷰트를 읽어야 하는데 그건 복제되지 않는다.
	 * 그러면 남의 판정만 확률이 틀리게 표시된다.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	FBGOutcomeWeights Weights;

	/** 효과의 실제 결과("피해 3" 등). 보너스 · 방어까지 반영된 값이다. 효과가 없었으면 빈 문자열. */
	UPROPERTY(BlueprintReadOnly, Category = "BG|Resolution")
	FString EffectSummary;
};

/**
 * 카드 효과 1개의 종류 (GDD §4.3 "등급별 효과").
 *
 * 한 등급이 여러 효과를 가질 수 있어야 한다 — 예) 속공 대성공은
 * "카드 1장 뽑기 + 피해 1 + 카드 사용한도 1 회복" 세 개다.
 * 그래서 등급마다 수치 하나만 두는 방식으로는 표현할 수 없다(그 방식은 없앴다).
 */
UENUM(BlueprintType)
enum class EBGCardEffectOp : uint8
{
	None                UMETA(DisplayName = "없음"),
	DamageTarget        UMETA(DisplayName = "대상에게 피해"),
	HealSelf            UMETA(DisplayName = "자신 회복"),
	DrawSelf            UMETA(DisplayName = "자신 카드 뽑기"),
	DiscardTargetRandom UMETA(DisplayName = "대상 손패 무작위 버리기"),
	StealTargetRandom   UMETA(DisplayName = "대상 손패 무작위 훔치기"),
	RestoreCardUse      UMETA(DisplayName = "카드 사용한도 회복"),
	ApplyStatus         UMETA(DisplayName = "\"다음 1회\" 상태 부여"),

	//	⚠️ 새 연산은 **맨 뒤에** 붙일 것. 중간에 끼우면 DT 에 이미 채워 둔 값이 통째로 밀린다.

	//	장비(파란 카드)를 무작위로 떼어낸다. 손패 연산과 대상이 다르다 —
	//	장비가 없는 상대에게 쓰면 아무 일도 일어나지 않는다(손패는 멀쩡히 남는다).
	DiscardTargetEquipRandom UMETA(DisplayName = "대상 장비 무작위 버리기"),
	StealTargetEquipRandom   UMETA(DisplayName = "대상 장비 무작위 훔치기")
};

/** 효과 1개. Amount 의 의미는 Op 에 따라 다르다(피해량 / 장수 / 회복 횟수). */
USTRUCT(BlueprintType)
struct FBGCardEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EBGCardEffectOp Op = EBGCardEffectOp::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	int32 Amount = 1;

	//	ApplyStatus 전용 — 부여할 Status.* 태그.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag StatusTag;

	/**
	 * ApplyStatus 전용 — true 면 대상에게, false 면 시전자에게 부여한다.
	 *   함정 = true (상대의 다음 카드를 방해)
	 *   약점 포착 / 준비 / 대비 = false (자기 강화)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bToTarget = false;
};

/**
 * 등급별 효과 목록 (GDD §4.3).
 *
 * 비어 있는 등급은 "효과 없음" 이다 — GDD §4.1 의 실패 기본안, §4.4 의 "대실패가 모든 카드에
 * 불이익을 강제하지 않는다" 를 그대로 표현한다.
 */
USTRUCT(BlueprintType)
struct FBGOutcomeEffects
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBGCardEffect> CriticalFailure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBGCardEffect> Failure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBGCardEffect> Success;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FBGCardEffect> CriticalSuccess;

	const TArray<FBGCardEffect>& ForOutcome(EBGDiceOutcome Outcome) const
	{
		switch (Outcome)
		{
		case EBGDiceOutcome::CriticalSuccess: return CriticalSuccess;
		case EBGDiceOutcome::Success:         return Success;
		case EBGDiceOutcome::Failure:         return Failure;
		default:                                return CriticalFailure;
		}
	}

	/** 어느 등급에도 효과가 없으면 true — 이 카드는 효과 목록 방식을 쓰지 않는다. */
	bool IsEmpty() const
	{
		return CriticalFailure.IsEmpty() && Failure.IsEmpty()
			&& Success.IsEmpty() && CriticalSuccess.IsEmpty();
	}
};

/**
 * 판정 등급별 결과 문구 (선택). 판정 게이지의 결과창이 띄운다.
 *
 * 비워 두면 효과에서 자동으로 만든 문구("피해 3")가 그대로 나온다 — 기존 카드는 채울 필요가 없다.
 * 자리표시자: {Amount} = 이 등급 효과의 수치, {Effect} = 자동 생성 문구.
 *   예) "급소를 맞혔다! 피해 {Amount}"
 *
 * ⚠️ 문구를 적어도 규칙은 달라지지 않는다. 실제로 일어나는 일은 OutcomeEffects 가 정한다 —
 *    둘이 어긋나면 화면만 거짓말을 한다.
 */
USTRUCT(BlueprintType)
struct FBGOutcomeTexts
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText CriticalFailure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Failure;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Success;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText CriticalSuccess;

	const FText& ForOutcome(EBGDiceOutcome Outcome) const
	{
		switch (Outcome)
		{
		case EBGDiceOutcome::CriticalSuccess: return CriticalSuccess;
		case EBGDiceOutcome::Success:         return Success;
		case EBGDiceOutcome::Failure:         return Failure;
		default:                              return CriticalFailure;
		}
	}
};

/**
 * 캐릭터에 붙는 무기/장비 메시 1개.
 *
 * SocketName 은 소켓 이름과 뼈(Bone) 이름을 둘 다 받는다 — AttachToComponent 가 소켓이
 * 있으면 그 소켓의 보정된 트랜스폼을, 없으면 그냥 그 이름의 뼈 트랜스폼을 쓰기 때문이다.
 * 자주 쓰고 정밀하게 맞춰야 하는 부위(양손 등)는 스켈레톤에 소켓을 만들어 쓰고, 당장 쓸지
 * 불확실한 부위는 소켓 없이 뼈 이름을 바로 적어 쓰면 된다 — 나중에 소켓으로 승격해도
 * 이름이 같으면 이 데이터는 그대로 유효하다.
 *
 * LocationOffset/RotationOffset 은 그 소켓/뼈 기준 로컬 공간이다(월드나 캐릭터 기준이 아니다).
 * 뼈에 직접 붙이는 경우 보정 없는 기본값(0)은 거의 항상 어색하다 — 소켓이 주는 "미리 맞춰둔
 * 축"이 없기 때문이다. 손잡이 위치·쥐는 각도는 메시를 고치지 않고 이 값으로 흡수한다.
 */
USTRUCT(BlueprintType)
struct FBGWeaponAttachment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FName SocketName = TEXT("WeaponSocket_R");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FRotator RotationOffset = FRotator::ZeroRotator;
};

/** 직업별 기본 공격과 판정 결과 Niagara 세트. */
USTRUCT(BlueprintType)
struct FBGCharacterAttackFX
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<UNiagaraSystem> AttackEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<UNiagaraSystem> DamageSuccessEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	TSoftObjectPtr<UNiagaraSystem> DamageCriticalEffect;
};

/**
 * 직업 1종의 GAS 구성과 외형/공격 연출을 서술하는 DT Row.
 *   - Row 이름 = CharacterTag 의 태그 문자열 (예: "Character.Job.Mage").
 *     DataSubsystem 이 태그로 이 Row 를 찾아 캐릭터에 부여한다.
 *   - 역할(Role.*) Row 는 만들지 않는다. 역할은 승리 조건 전용이라 스탯도 능력도 갖지 않는다.
 *
 * 매핑
 *   DefaultAttributeGE  : Health / CardUseLimit 등 기본 스탯을 세팅하는 GE(Instant).
 *   PassiveEffects      : 직업 정체성 패시브 GE(Infinite).
 *   GrantedAbilities    : 직업 고유 능동 GA.
 *   SkeletalMesh        : 직업 외형.
 *   WeaponAttachments   : 직업이 손에 쥐는(또는 몸에 붙는) 무기/장비 메시 목록. 비우면 맨손.
 *   AttackFX            : CharacterAttack 정책 카드가 공유하는 직업별 Niagara 세트.
 */
USTRUCT(BlueprintType)
struct FBGCharacterRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 이 Row 를 식별하는 태그 (Character.Job.*).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CharacterTag;

	// 화면에 보여줄 직업 이름. 비워 두면 태그의 마지막 마디를 쓴다
	// (Character.Job.Mage → "Mage") — 이름을 코드에 박지 않기 위한 폴백이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	// 이 직업이 무엇을 하는지 한 줄 설명(턴 패널 툴팁이 읽는다).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	/** Role-reveal card artwork for this job. Text and secret role remain dynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|UI")
	TSoftObjectPtr<UTexture2D> IntroCardTexture;

	// 기본 스탯 세팅 GE (Instant). Health/MaxHealth/WeaponRange/BangLimit ... 를 초기화.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> DefaultAttributeGE;

	// 캐릭터 특수능력 패시브 GE (Infinite). 역할 변경 시 함께 회수된다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<UGameplayEffect>> PassiveEffects;

	// 이 캐릭터에게 부여할 능동 어빌리티 (GA).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<UGameplayAbility>> GrantedAbilities;

	// ── 외형 (직업 Row 전용) ─────────────────────────────────────
	//
	// 직업 태그가 배정되면 ABGCharacter 가 GetMesh() 에 이 메쉬를 꽂는다.
	// 비워 두면 BP_BGCharacter 의 기본 메쉬를 그대로 쓴다 — 메쉬가 아직
	// 없어도 배정과 스탯 부여는 정상 동작한다.
	//
	// ⚠️ 소프트 참조다. 8종을 전부 하드 참조하면 캐릭터 Row 를 읽는 순간
	//    쓰지도 않을 직업 메쉬까지 통째로 메모리에 올라간다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	// 메쉬를 교체하면 AnimInstance 가 초기화된다. 직업 전용 ABP 가 있으면 여기에 넣는다.
	// 비워 두면 BP 기본 ABP 를 다시 물린다(스켈레톤이 호환될 때만 정상 동작).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TSubclassOf<UAnimInstance> AnimClass;

	// 손에 쥐는(또는 몸에 붙는) 무기/장비 메시들. 항목 하나가 메시+소켓(또는 뼈)+오프셋 1벌이다.
	// 비워 두면 맨손 — 정상 상태다(수도사 등). 듀얼 웨폰·무기+방패처럼 여러 개를 동시에
	// 붙여야 하면 항목을 그만큼 추가하면 된다 — ABGCharacter 가 항목 수만큼 컴포넌트를 만든다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TArray<FBGWeaponAttachment> WeaponAttachments;

	/** Bang·QuickStrike 등 CharacterAttack 정책 카드가 공유하는 직업별 연출. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual|AttackFX")
	FBGCharacterAttackFX AttackFX;

	// 이 직업을 고른 플레이어를 나타내는 색. 핑 테두리 등 "누가 했는지" 표시에 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	FLinearColor PlayerColor = FLinearColor::White;
};

/**
 * 카드 1종의 등장 확률(가중치). 확률 기반 카드 분배용.
 *   - Row 이름 = CardId 태그 문자열 (예: "Card.Id.Bang") 로 두면 관리가 편하다.
 *   - Weight 는 "상대 가중치" — 전부 합해서 1 일 필요 없다. 클수록 자주 뽑히고, 0 이면 안 나온다.
 *     예) Bang=25, Missed=12, Beer=6 처럼 넣으면 실제 덱 비율과 비슷해진다.
 */
USTRUCT(BlueprintType)
struct FBGCardProbabilityRow : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 어떤 카드인가 (Card.Id.* 태그).
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag CardId;

	// 뽑힐 상대 가중치(확률). 0 이면 등장하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};
/**
 * 덱을 어느 진영 것으로 다룰지 (PvE 전용).
 * PvP 는 덱이 하나뿐이라 항상 Player 다.
 */
UENUM(BlueprintType)
enum class EBGDeckSide : uint8
{
	Player,
	Enemy
};

USTRUCT(BlueprintType)
struct FBGCardRow : public FTableRowBase
{
	GENERATED_BODY()
public:
	//	카드 종류 ( Card.Id.* )
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FGameplayTag CardIdTag;
	
	//	카드 이름
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FText DisplayName;
	//	카드 설명
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FText Description;
	
	/**
	 * 카드 그림 — 테두리 안쪽에 들어가는 이미지 (테두리는 위젯이 겹쳐 그린다).
	 *
	 * TSoftObjectPtr 인 이유: 하드 참조로 두면 DT_BGCard 를 여는 순간 **모든 카드의 그림이
	 * 통째로 메모리에 올라온다.** 손패에 실제로 들어온 카드만 로드하면 되므로 소프트 참조로 둔다.
	 * 실제 로드는 UBGCardViewLibrary::MakeCardView 가 뷰를 만들 때 한 장씩 한다.
	 *
	 * 비워 두면 위젯이 폴백을 그린다 — 조용히 투명해지지 않는다(MakeCardView 가 경고도 남긴다).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Art")
	TSoftObjectPtr<UTexture2D> ArtTexture;

	//	카드 타입 ( Card.Type.* )
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag TypeTag;
	
	//	카드 특성 ( Card.Trait.* )
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FGameplayTagContainer TraitTags;

	/** 대상 선택 중 표시할 선 연출 계열. 대상 선택이 없는 카드는 사용되지 않는다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	EBGTargetSelectionStyle TargetSelectionStyle = EBGTargetSelectionStyle::Basic;

	// 갈색 카드: 사용 시 이 이벤트 태그로 GA 를 트리거한다 (Ability.Bang 등).
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	FGameplayTag AbilityEventTag;

	// 파란 카드: 장착 시 적용할 Infinite GE.
	UPROPERTY(EditAnywhere, BlueprintReadOnly) 
	TSubclassOf<UGameplayEffect> EquipEffect;

	//	[미사용] GA/GE 가 읽는 범용 수치로 만들었지만 참조하는 코드가 없다.
	//	  등급별 수치는 OutcomeEffects 의 Amount 가 담당한다. 채워도 아무 일도 일어나지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Magnitude = 0;
	
	//	덱에 들어갈 카드 매수 (PvP 전용. PvE 는 아래 두 필드를 따로 쓴다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 QuantityOfCard = 0;

	//	PvE 전용. 플레이어/적 덱에 들어갈 매수, 0 이면 그 진영 덱엔 안 들어간다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 PlayerDeckQuantity = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 EnemyDeckQuantity = 0;

	//	[비활성] 확률을 사용하는 카드의 성공 판정 기준값.
	//	  OutcomeWeights 로 대체됐다 (GDD §4.2 — 4단계 비율 방식). 참조하지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0"))
	int32 CheckChance = 0;

	// ── 4단계 판정 (GDD §4.2 / §4.3) ──────────────────────────────

	//	판정 비율. 합이 100 일 필요 없다(정규화됨).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome")
	FBGOutcomeWeights OutcomeWeights;

	//	[미구현] 이 카드가 받을 수 있는 보정의 종류 (GDD §4.3 "보정 허용 태그").
	//	  ⚠️ 아직 아무도 읽지 않는다 — 채워도 보정이 걸러지지 않고 전부 적용된다.
	//	     거르려면 UBGDiceComponent::ApplyModifiers 에서 이 컨테이너를 보게 해야 한다.
	//	  비어 있으면 모든 보정을 받는다(구현 후에도 이 규칙 유지 예정).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome")
	FGameplayTagContainer ModifierTags;

	/**
	 * 등급별 효과 목록 — **모든 카드의 정식 경로**다.
	 *
	 * 한 등급에 여러 효과를 넣을 수 있다(속공·휴식 등). 파란 카드(장비)를 제외한
	 * 모든 갈색 카드가 이걸 채우고 UGA_BGCardEffects(범용 실행기)에 매핑된다.
	 *
	 * ⚠️ 비워 두면 카드를 써도 아무 일이 일어나지 않는다. BG_DumpDeck 이 경고한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome")
	FBGOutcomeEffects OutcomeEffects;

	/** 등급별 결과 문구(선택). 비워 두면 효과에서 자동으로 만든 문구를 쓴다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Outcome")
	FBGOutcomeTexts OutcomeTexts;
};
