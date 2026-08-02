// BANG! 캐릭터 — ASC/AttributeSet 를 소유하고, DataSubsystem 의 DT Row 를 받아
// 스탯 GE / 패시브 / 어빌리티를 자신에게 부여한다. (Team4Project 의 ABaseCharacter 축약판)
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

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UFUNCTION(BlueprintPure, Category = "BG")
	FGameplayTag GetCharacterTag() const { return CharacterTag; }

	// 서버에서 역할/캐릭터를 배정할 때 호출. 이미 GAS 초기화가 끝났으면 이전 부여분을
	// 정리하고 새 Row 로 재적용한다. 소유 클라에 복제된다.
	UFUNCTION(BlueprintCallable, Category = "BG")
	void SetCharacterTag(const FGameplayTag& NewTag);

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

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── GAS ──
	UPROPERTY(VisibleAnywhere, Category = "GAS")
	TObjectPtr<UBGAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBGAttributeSet> AttributeSet;

	// 이 캐릭터의 역할/특능 태그. Row 조회 키로 쓰인다.
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_CharacterTag, Category = "BG")
	FGameplayTag CharacterTag;

	UFUNCTION()
	void OnRep_CharacterTag();

	// ASC ActorInfo 초기화 (서버/클라 공통 경로).
	void InitializeAbilityActorInfo();

	// 서버 전용: 최초 GAS 부여. 중복 방지 플래그.
	void ServerInitGAS();
	bool bGASInitialized = false;

	// 서버 전용. 사망 1회 처리 보장.
	bool bDead = false;

	// DataSubsystem 에서 RowTag 로 Row 를 찾아 DefaultAttributeGE/PassiveEffects/GrantedAbilities 부여.
	void ApplyCharacterDataRow(const FGameplayTag& RowTag);
	// ApplyCharacterDataRow 가 부여한 GE/어빌리티를 모두 회수 (역할 변경 시).
	void ClearCharacterDataRow();

	// CharacterTag 를 ASC 루즈 태그로 동기화 (어빌리티의 ActivationRequiredTags 검사용).
	void RefreshCharacterLooseTag();
	FGameplayTag AppliedLooseTag;

	// 부여 핸들 추적 (재적용 시 정리).
	TArray<FActiveGameplayEffectHandle> AppliedEffectHandles;
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};
