// BANG! 캐릭터 — ASC/AttributeSet 를 소유하고, DataSubsystem 의 DT Row 를 받아
// 스탯 GE / 패시브 / 어빌리티를 자신에게 부여한다. (Team4Project 의 ABaseCharacter 축약판)
//
// 태그가 둘이고 역할이 완전히 다르다:
//   JobTag (Character.Job.*) — 스탯·능력·외형의 **유일한** 출처. DT Row 를 읽어 GAS 를 부여한다. 공개.
//   RoleTag (Role.*)    — 승리 조건 판정 전용. DT 를 조회하지 않는다.
//                              ASC 루즈 태그로만 올라가 승패/어빌리티 조건 검사에 쓰인다. 숨김.
//                              예외: 보안관은 규칙상 고정 보너스(MaxHealth/Health +1)를 받는다
//                              (SetRoleTag 참고).
//
// ⚠️ 설계 메모: Prototype-Workflow.md §1~2 는 "폰 없이 PlayerState 에 ASC" 를 권장한다
//    (손패 은닉 정보 복제 때문). 이 클래스는 유저 요청대로 Team4 방식(Character 소유 ASC)을
//    따른다. 좌석 위젯만 쓰는 최종 형태로 갈 때 ASC 소유를 PlayerState 로 옮기면 된다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayAbilitySpecHandle.h"
#include "Core/BGGameState.h"   // EBGCinematicStage / FBGCardResolution — ABP 에 노출하는 값이라 완전한 타입이 필요하다
#include "BGCharacter.generated.h"

class UBGAbilitySystemComponent;
class UBGAttributeSet;
class UCameraComponent;
class UGameplayAbility;
class UGameplayEffect;
class UBGStatusFXComponent;
class UStaticMeshComponent;

/**
 * ABP 가 읽는 이 캐릭터의 연출 상태 (아트 기획서 "전투 연출").
 *
 * ⚠️ 규칙에 관여하지 않는다. 판정·효과·카드 소비는 이 값이 바뀌기 전에 이미 서버에서 끝나 있다.
 *    애니메이션이 하나도 없어도 게임은 똑같이 돌아간다.
 *
 * 값은 복제되지 않는다 — ABGGameState 의 복제된 연출 상태에서 **각자 유도한다.**
 * 캐릭터마다 따로 복제하면 7인분이 중복되고, 무엇보다 GameState 와 어긋난 순간이 생긴다.
 */
UENUM(BlueprintType)
enum class EBGCharacterAnimState : uint8
{
	//	연출 중이 아니다. 평상시 상태.
	Idle  UMETA(DisplayName = "대기"),

	//	내가 카드를 쓴 사람이고 지금이 Primary 단계다 — 공격/시전 동작.
	Cast  UMETA(DisplayName = "카드 사용"),

	//	내가 이번 연출의 대상이고 지금이 Secondary 단계다 — 피격/회피 동작.
	//	성패는 GetCinematicOutcome() 으로 갈라 쓴다(성공이면 맞고, 실패면 흘린다).
	React UMETA(DisplayName = "결과 반응"),

	//	연출 중이지만 내 차례가 아니다 — 주인공 쪽을 보는 등의 방관 동작.
	//	쓸 동작이 없으면 ABP 에서 Idle 과 같은 상태로 묶어도 된다.
	Watch UMETA(DisplayName = "관전"),

	//	사망 연출이 모두 끝난 뒤의 시체. 포즈를 유지한다.
	Dead  UMETA(DisplayName = "사망"),

	// ⚠️ 아래는 나중에 추가된 값이다. **순서를 바꾸지 말 것** — ABP 가 이미 위 다섯을 참조한다.

	//	사망 연출 1단계 — 내가 이번 사망자다. 쓰러지는 동작.
	//	피격(React)과 다르다: 피격은 카드 연출에서 죽든 말든 재생되고,
	//	쓰러지는 것은 그 카드 연출이 다 끝난 뒤 별도 연출로 온다.
	Dying UMETA(DisplayName = "쓰러짐"),

	//	사망 연출 2단계 — 역할 공개. 이 상태에 들어온 시점에 PublicRoleTag 가 이미 채워져 있다.
	//	진입과 동시에 RefreshRoleRevealVisual() 이 본체 메시를 숨기고 정체 공개 메시로 바꾼다.
	RoleReveal UMETA(DisplayName = "역할 공개")
};

/** 연출 상태가 바뀌었다. 몽타주처럼 "한 번만" 재생해야 하는 것은 이걸 구독한다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGCharacterAnimStateChanged,
	EBGCharacterAnimState, NewState);

UCLASS()
class BG_API ABGCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABGCharacter();

	// 서버: 컨트롤러 점유 시 ASC ActorInfo 초기화 + GAS 최초 부여.
	virtual void PossessedBy(AController* NewController) override;
	// 클라(소유): 컨트롤러 복제 도착 시 ASC ActorInfo 초기화 (UI 가 어트리뷰트를 읽을 수 있게).
	virtual void OnRep_Controller() override;

	// 이미 직업이 정해진 뒤에 관련성이 생긴 클라를 위한 외형 보정 + 연출 상태 구독.
	virtual void BeginPlay() override;

	// GameState 구독을 끊는다. 약참조라 없어도 크래시는 안 나지만 죽은 항목이 남는다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "BG")
	FGameplayTag GetRoleTag() const { return RoleTag; }

	// 서버에서 역할을 배정할 때 호출. 태그를 저장하고 ASC 루즈 태그에 반영한다.
	// 스탯/능력은 원칙적으로 직업(SetJobTag)의 몫이지만, 보안관 역할만 예외로
	// 고정 보너스(ApplySheriffBonusIfNeeded)를 함께 적용한다. 소유 클라에만 복제된다.
	UFUNCTION(BlueprintCallable, Category = "BG")
	void SetRoleTag(const FGameplayTag& NewTag);

	// 보안관 전용 고정 보너스(MaxHealth/Health +1, GE_BGSheriffBonus)를 적용한다.
	// SetRoleTag 가 내부에서 자동으로 부른다. 지금 RoleTag 가 보안관이 아니면 아무 일도 안 한다.
	UFUNCTION(BlueprintCallable, Category = "BG")
	void ApplySheriffBonusIfNeeded();

	// PvE 조우 승리 보상. 이긴 조우 수만큼 보안관의 최대 체력과 현재 체력을 +2 씩 올린다.
	// SetRoleTag 가 부른다. 보안관이 아니거나 PvE 가 아니거나 아직 이긴 조우가 없으면 아무 일도 안 한다.
	void ApplyEncounterHealthBonusIfNeeded();

	// 위 보상으로 올라간 체력 총량(서버 전용). 시작 손패 장수처럼 "보상 전 체력" 이 필요한 계산이 뺄 때 쓴다.
	int32 GetEncounterHealthBonus() const { return EncounterHealthBonus; }

	// 이 캐릭터의 직업 태그 (Character.Job.*). 공개 정보라 전원에게 복제된다.
	UFUNCTION(BlueprintPure, Category = "BG")
	FGameplayTag GetJobTag() const { return JobTag; }

	UFUNCTION(BlueprintPure, Category = "BG|FX")
	UBGStatusFXComponent* GetStatusFX() const { return StatusFX; }

	/**
	 * 서버에서 직업을 배정할 때 호출. DT Row 를 읽어 스탯 GE/패시브/어빌리티를 부여하고,
	 * Row 의 SkeletalMesh 로 캐릭터 외형까지 교체한다.
	 *
	 * 이 캐릭터의 스탯은 전부 여기서 나온다 — Row 가 비어 있으면 스탯도 비어 있다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG")
	void SetJobTag(const FGameplayTag& NewTag);

	/**
	 * 서버 전용. 적 AI를 PvE 보스로 지정한다. 새 직업을 만들지 않고, 이미 적용된
	 * 직업 스탯 위에 State.Boss 태그와 StatBoostGE(스탯 배율 GameplayEffect)를 얹는다.
	 * 반드시 SetJobTag 이후에 불러야 한다(순서가 바뀌면 강화가 덮어써진다).
	 */
	void MarkAsBoss(TSubclassOf<UGameplayEffect> StatBoostGE);

	/**
	 * 서버 전용. Health 가 0 이 되었음을 AttributeSet 이 알려온다.
	 * 값 판정은 AttributeSet 이, 사망이라는 "규칙 사건" 은 여기서부터 처리한다.
	 * 이미 죽었으면 무시한다(회복→재사망 중복 방지).
	 *
	 * Killer : 피해 GE 의 Instigator. 보상/벌칙 판정에 쓴다. 없을 수 있다.
	 */
	void NotifyHealthDepleted(AActor* Killer);

	/**
	 * 서버 전용. 피해를 받았음을 AttributeSet 이 알려온다 (죽었는지와 무관하게).
	 *
	 * 여기서 처리하는 것은 "맞았을 때 되받는" 직업 패시브 둘이다 —
	 * 수도사(DamageReflect: 가해자에게 피해) / 도둑(DamageStealCount: 가해자에게서 카드).
	 *
	 * ⚠️ 반사가 다시 반사를 부르는 것을 막는다(수도사끼리 마주 보면 무한 루프).
	 *    구현부의 재진입 걸쇠 참고.
	 *
	 * DamageAmount : 실제로 깎인 체력. 반사량은 이 값과 무관하게 어트리뷰트가 정한다.
	 */
	void NotifyDamaged(AActor* DamageInstigator, int32 DamageAmount);

	UFUNCTION(BlueprintPure, Category = "BG")
	bool IsDead() const { return bDead; }

	// ══════════════════════════════════════════════════════════════════
	//  카메라 (연출) — 구 ABGPlayerBase 에서 통합
	//
	//  A(Primary) : 평상시 각자 자기 캐릭터를 통해 보는 시점.
	//  B(Secondary): 연출용 샷의 **위치·FOV 마커**다. 실제로 이 카메라로 보지 않는다 —
	//                ABGCinematicCamera 가 트랜스폼만 읽어가 그 자리로 날아온다.
	//
	//  ⚠️ ACharacter 는 CapsuleComponent 가 루트다. 두 카메라는 그 밑에 붙으므로
	//     캡슐이 회전하면 함께 돈다. 캡슐과 무관하게 고정하고 싶으면 해당 컴포넌트에
	//     SetUsingAbsoluteRotation(true) 를 주면 된다.
	// ══════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "BG|Camera")
	UCameraComponent* GetPrimaryCamera() const { return PrimaryCamera; }

	UFUNCTION(BlueprintPure, Category = "BG|Camera")
	UCameraComponent* GetSecondaryCamera() const { return SecondaryCamera; }

	/**
	 * 두 카메라 중 하나만 활성 상태로 만든다.
	 *
	 * ⚠️ 둘 다 활성이면 AActor::CalcCamera 가 OwnedComponents(TSet)에서 **먼저 찾은 것**을 쓴다.
	 *    순회 순서가 보장되지 않으므로 "가끔 다른 시점으로 시작하는" 재현 어려운 버그가 된다.
	 *    전환은 반드시 이 함수를 거칠 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Camera")
	void SetActiveCamera(bool bUsePrimary);

	UFUNCTION(BlueprintPure, Category = "BG|Camera")
	bool IsPrimaryCameraActive() const;

	/**
	 * 서버 전용. 캐릭터 **몸**을 이 방향으로 돌린다 (좌석 배치에서 캠프파이어를 보게 할 때).
	 *
	 * ⚠️ 서버에서 SetActorRotation 만 하면 **자기 캐릭터를 조종하는 클라에는 반영되지 않는다.**
	 *    캐릭터 이동은 클라 권위(서버는 보정만)인데, 그 보정 패킷(ClientAdjustPosition)에는
	 *    위치·속도만 들어가고 회전은 없다. 그래서 위치는 맞는데 회전만 안 따라오는 현상이 난다.
	 *    소유 클라에는 아래 Client RPC 로 따로 알린다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Seating")
	void SetBodyFacing(FRotator NewRotation);

	// ══════════════════════════════════════════════════════════════════
	//  연출 애니메이션 상태 (ABP 가 읽는 면)
	//
	//  ── 왜 캐릭터가 들고 있나 ─────────────────────────────────────
	//  ABP 는 매 프레임 자기 폰만 본다. GameState 를 직접 찾아 "내가 시전자인가" 를
	//  매번 따지게 하면 그 판단이 ABP 마다 복제되고, 직업별 ABP 가 늘어날수록 어긋난다.
	//  판단은 여기 한 곳에서 하고 ABP 는 결과만 읽는다.
	//
	//  ── 두 가지 읽는 법 ──────────────────────────────────────────
	//    상태 머신 : GetAnimState() 를 매 프레임 읽어 전이 조건으로 쓴다.
	//    몽타주    : OnAnimStateChanged 를 구독해 진입 순간에 한 번 재생한다.
	//                (매 프레임 읽어 재생하면 몽타주가 계속 처음부터 다시 시작한다)
	//
	//  값은 서버·클라 양쪽에서 같은 시점에 갱신된다 — GameState 의 연출 단계 통지가
	//  두 곳 모두에서 오기 때문이다.
	// ══════════════════════════════════════════════════════════════════

	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	EBGCharacterAnimState GetAnimState() const { return AnimState; }

	/** 지금 판 전체의 연출 단계. 내 역할과 무관하다 — 그건 GetAnimState() 가 답한다. */
	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	EBGCinematicStage GetCinematicStage() const { return CinematicStage; }

	/** 연출을 일으킨 카드 (Card.Id.*). 카드별로 다른 동작을 고를 때 쓴다. 연출 밖에서는 무효 태그. */
	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	FGameplayTag GetCinematicCardId() const { return CinematicCardId; }

	/**
	 * 그 카드의 판정 결과. 피격이냐 회피냐를 여기서 가른다.
	 *
	 * ⚠️ HasCinematicOutcome() 이 false 면 이 값은 의미가 없다(판정을 굴리지 않은 카드).
	 *    검사 없이 쓰면 장비 카드가 항상 "성공" 으로 보인다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	EBGDiceOutcome GetCinematicOutcome() const { return CinematicOutcome; }

	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	bool HasCinematicOutcome() const { return bHasCinematicOutcome; }

	/** 지금 연출 카메라가 나를 비추고 있는가. 얼굴 표정·상체 강조처럼 화면에 잡힐 때만 켤 것에 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	bool IsCinematicSubject() const { return bCinematicSubject; }

	/**
	 * 쓰러져 있어야 하는가 (사망 연출 중 포함).
	 *
	 * ABP 의 사망 상태 전이 조건으로 이걸 쓸 것. `AnimState == Dead` 로만 걸면
	 * 쓰러지는 몽타주가 끝나는 순간(Dying / RoleReveal 구간) 블렌드 아웃되어
	 * **시체가 일어섰다가 다시 눕는다.**
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Anim")
	bool IsDownedPose() const
	{
		return AnimState == EBGCharacterAnimState::Dying
			|| AnimState == EBGCharacterAnimState::RoleReveal
			|| AnimState == EBGCharacterAnimState::Dead;
	}

	/** 상태 진입 통지. 몽타주 재생용 — 상태 머신은 이걸 쓰지 말고 GetAnimState() 를 폴링할 것. */
	UPROPERTY(BlueprintAssignable, Category = "BG|Anim")
	FOnBGCharacterAnimStateChanged OnAnimStateChanged;

	/**
	 * 연출 상태를 GameState·PlayerState 에서 다시 읽어 온다.
	 *
	 * 평소에는 알아서 불린다(연출 단계 변경·사망 복제). 직접 부를 일은
	 * 애니메이션 디버깅이나 폰을 늦게 붙였을 때뿐이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Anim")
	void RefreshAnimState();

	/**
	 * 정체가 드러나야 하는 상태(RoleReveal 이후)인지 보고 본체 메시 ↔ 정체 공개 메시를 맞바꾼다.
	 * 이미 스왑된 뒤(RevealMeshComponent 에 메시가 꽂혀 있음)면 조용히 아무 일도 하지 않는다.
	 *
	 * ⚠️ 이 함수는 세 곳에서 재시도된다 — 셋 다 정상 흐름이고, 어느 하나만으로는 늦게
	 *    도착하는 값(PublicRoleTag)이나 놓친 순간을 못 잡을 수 있어 서로를 보완한다.
	 *    1. **애님 노티파이** — 쓰러짐/역할공개 몽타주에 심어 둔 `UBGAnimNotify_RoleReveal` 이
	 *       원하는 프레임에서 부른다(연기 이펙트 등과 타이밍을 맞추는 주 경로).
	 *    2. **`Dead` 상태 진입** — RefreshAnimState 의 안전망. 몽타주를 아예 보지 못한
	 *       늦은 접속자를 위한 것이다.
	 *    3. **`ABGPlayerState::OnRep_PublicRole`** — CinematicStage(GameState)와 PublicRoleTag
	 *       (PlayerState)는 액터가 달라 복제 도착 순서가 보장되지 않는다. 위 두 재시도가
	 *       이미 지나간 뒤에 값이 도착해도 이 OnRep 이 다시 불러 준다.
	 *    죽었다가 되살아나는 경우는 없으므로 되돌리는 코드는 두지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Anim")
	void RefreshRoleRevealVisual();

protected:
	/** 소유 클라에서 몸 방향을 맞춘다. 서버의 SetBodyFacing 이 호출한다. */
	UFUNCTION(Client, Reliable)
	void Client_SetBodyFacing(FRotator NewRotation);

	// ── 연출 애니메이션 내부 ──────────────────────────────────────

	//	GameState 의 연출 단계 통지를 구독한다. GameState 가 이 폰보다 늦게 올 수 있어
	//	BeginPlay 에서 실패하면 UWorld::GameStateSetEvent 로 한 번 더 노린다.
	void BindCinematicDelegate();

	//	GameState 통지 수신부. 단계와 주인공은 여기서 쓰지 않고 RefreshAnimState 가 다시 읽는다 —
	//	사망 복제처럼 통지 없이 바뀌는 값도 같은 함수 하나로 반영하기 위해서다.
	void HandleCinematicStageChanged(EBGCinematicStage Stage, ABGCharacter* Subject);

	//	현재 GameState/PlayerState 로부터 상태 하나를 유도한다. 부수 효과 없음.
	EBGCharacterAnimState ComputeAnimState(const class ABGGameState* GS) const;

	//	중복 구독 방지 겸 해제 대상. 약참조라 GameState 가 사라져도 안전하다.
	TWeakObjectPtr<class ABGGameState> BoundCinematicGameState;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	EBGCharacterAnimState AnimState = EBGCharacterAnimState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	EBGCinematicStage CinematicStage = EBGCinematicStage::None;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	FGameplayTag CinematicCardId;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	EBGDiceOutcome CinematicOutcome = EBGDiceOutcome::Success;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	bool bHasCinematicOutcome = false;

	UPROPERTY(BlueprintReadOnly, Category = "BG|Anim")
	bool bCinematicSubject = false;

	/** 평상시 시점. 스폰 시 이쪽이 활성이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|FX")
	TObjectPtr<UBGStatusFXComponent> StatusFX;

	/** 정체 공개 연출에서 본체 대신 보이는 시체 메시. 평소엔 숨겨져 있고 메시도 비어 있다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Anim")
	TObjectPtr<UStaticMeshComponent> RevealMeshComponent;

	/**
	 * 직업의 무기/장비 메시들(FBGCharacterRow::WeaponAttachments). 항목 수만큼 런타임에
	 * 만들어진다 — 생성자에서 고정 개수로 만들 수 없다(직업마다 몇 개가 필요한지 다르다).
	 * ApplyJobVisual 이 매번 전부 파괴하고 새로 만든다(직업 배정은 판마다 1회뿐이라 문제없다).
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "BG|Visual")
	TArray<TObjectPtr<UStaticMeshComponent>> EquipmentMeshComponents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Camera")
	TObjectPtr<UCameraComponent> PrimaryCamera;

	/** 연출 샷 마커. 기본 비활성이며, 연출 중에도 활성화하지 않는다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Camera")
	TObjectPtr<UCameraComponent> SecondaryCamera;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── GAS ──
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UBGAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBGAttributeSet> AttributeSet;

	// ── 역할 (Role.*) ──
	// 승리 조건 판정용. DT 를 조회하지 않는다 — 이 태그로 부여되는 스탯/능력은 없다.
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_RoleTag, Category = "BG")
	FGameplayTag RoleTag;

	// ApplyEncounterHealthBonusIfNeeded 로 올라간 체력 총량. 서버만 쓴다(복제하지 않는다).
	int32 EncounterHealthBonus = 0;

	UFUNCTION()
	void OnRep_RoleTag();

	// ── 직업 (Character.Job.*) ──
	// DT Row 조회 키이자 외형 교체 키. 이 캐릭터의 스탯·능력·메쉬가 전부 여기서 나온다.
	// ⚠️ RoleTag 와 달리 조건 없이 전원에게 복제된다 — 직업은 공개 정보고(GDD §7),
	//    무엇보다 남의 캐릭터 메쉬가 바뀌려면 그 클라도 태그를 받아야 한다.
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_JobTag, Category = "BG")
	FGameplayTag JobTag;

	UFUNCTION()
	void OnRep_JobTag();

	// ASC ActorInfo 초기화 (서버/클라 공통 경로).
	void InitializeAbilityActorInfo();

	// 서버 전용: 최초 GAS 부여. 중복 방지 플래그.
	void ServerInitGAS();
	bool bGASInitialized = false;

	// 서버 전용. 사망 1회 처리 보장.
	bool bDead = false;

	// ── 직업 Row 적용 ──

	// DataSubsystem 에서 JobTag 로 Row 를 찾아 DefaultAttributeGE/PassiveEffects/GrantedAbilities 부여.
	// Row 가 없으면 경고한다 — 스탯의 유일한 출처라 비어 있으면 캐릭터가 성립하지 않는다.
	void ApplyJobDataRow();

	// ApplyJobDataRow 가 부여한 GE/어빌리티를 회수 (직업 재배정 시).
	void ClearJobGrants();

	// 부여 핸들 = "떼어낼 때 쓰는 영수증". 이게 없으면 붙인 패시브/어빌리티를 되돌릴 수 없어
	// 직업을 바꿔도 이전 직업의 효과가 그대로 남는다. GE 와 어빌리티는 핸들 타입이 달라 배열이 둘이다.
	// (Instant GE 는 무효 핸들을 돌려준다 — 뗄 대상이 없으므로 담지 않는다)
	TArray<FActiveGameplayEffectHandle> JobEffectHandles;
	TArray<FGameplayAbilitySpecHandle>  JobAbilityHandles;

	// 직업 Row 의 SkeletalMesh/AnimClass 를 GetMesh() 에 반영. 서버·클라 모두에서 돈다.
	void ApplyJobVisual();

	// ── 루즈 태그 동기화 ──
	//
	// 역할과 직업 둘 다 ASC 에 올려야 한다 (어빌리티의 ActivationRequiredTags / 승패 판정이 읽는다).
	// 슬롯을 축마다 따로 두는 이유: 슬롯이 하나면 직업 태그를 붙이는 순간 역할 태그가 떨어진다.
	void RefreshLooseTags();
	void SyncLooseTag(const FGameplayTag& DesiredTag, FGameplayTag& AppliedSlot);
	FGameplayTag AppliedRoleLooseTag;
	FGameplayTag AppliedJobLooseTag;
};
