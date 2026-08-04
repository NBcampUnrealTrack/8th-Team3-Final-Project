// BANG! 캐릭터 — ASC/AttributeSet 를 소유하고, DataSubsystem 의 DT Row 를 받아
// 스탯 GE / 패시브 / 어빌리티를 자신에게 부여한다. (Team4Project 의 ABaseCharacter 축약판)
//
// 태그가 둘이고 역할이 완전히 다르다:
//   JobTag (Character.Job.*) — 스탯·능력·외형의 **유일한** 출처. DT Row 를 읽어 GAS 를 부여한다. 공개.
//   RoleTag (Role.*)    — 승리 조건 판정 전용. DT 를 조회하지 않고 GAS 도 부여하지 않는다.
//                              ASC 루즈 태그로만 올라가 승패/어빌리티 조건 검사에 쓰인다. 숨김.
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
#include "BGCharacter.generated.h"

class UBGAbilitySystemComponent;
class UBGAttributeSet;
class UCameraComponent;
class UGameplayAbility;
class UGameplayEffect;

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

	// 이미 직업이 정해진 뒤에 관련성이 생긴 클라를 위한 외형 보정.
	virtual void BeginPlay() override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "BG")
	FGameplayTag GetRoleTag() const { return RoleTag; }

	// 서버에서 역할을 배정할 때 호출. 태그를 저장하고 ASC 루즈 태그에 반영할 뿐,
	// 스탯/능력은 건드리지 않는다 — 그건 직업(SetJobTag)의 몫이다. 소유 클라에만 복제된다.
	UFUNCTION(BlueprintCallable, Category = "BG")
	void SetRoleTag(const FGameplayTag& NewTag);

	// 이 캐릭터의 직업 태그 (Character.Job.*). 공개 정보라 전원에게 복제된다.
	UFUNCTION(BlueprintPure, Category = "BG")
	FGameplayTag GetJobTag() const { return JobTag; }

	/**
	 * 서버에서 직업을 배정할 때 호출. DT Row 를 읽어 스탯 GE/패시브/어빌리티를 부여하고,
	 * Row 의 SkeletalMesh 로 캐릭터 외형까지 교체한다.
	 *
	 * 이 캐릭터의 스탯은 전부 여기서 나온다 — Row 가 비어 있으면 스탯도 비어 있다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG")
	void SetJobTag(const FGameplayTag& NewTag);

	/**
	 * 서버 전용. Health 가 0 이 되었음을 AttributeSet 이 알려온다.
	 * 값 판정은 AttributeSet 이, 사망이라는 "규칙 사건" 은 여기서부터 처리한다.
	 * 이미 죽었으면 무시한다(회복→재사망 중복 방지).
	 *
	 * Killer : 피해 GE 의 Instigator. 보상/벌칙 판정에 쓴다. 없을 수 있다.
	 */
	void NotifyHealthDepleted(AActor* Killer);

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

protected:
	/** 소유 클라에서 몸 방향을 맞춘다. 서버의 SetBodyFacing 이 호출한다. */
	UFUNCTION(Client, Reliable)
	void Client_SetBodyFacing(FRotator NewRotation);

	/** 평상시 시점. 스폰 시 이쪽이 활성이다. */
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
