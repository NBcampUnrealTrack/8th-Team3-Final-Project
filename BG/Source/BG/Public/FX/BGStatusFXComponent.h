// BANG! 버프/디버프 이펙트 — 캐릭터에 계속 붙어 있는 지속 이펙트를 관리한다.
//
// ── 이 컴포넌트의 계약: "스폰/제거" 가 아니라 "재조정" 이다 ────────────────────────
//
//  켜라/꺼라 이벤트를 받지 않는다. 복제된 상태에서 **지금 켜져 있어야 할 집합**을 매번
//  다시 계산하고, 지금 켜져 있는 집합과 비교해 차이만 반영한다. 몇 번 불려도 결과가 같다.
//
//  이 형태여야 하는 이유가 셋이다.
//    · 늦게 들어온 클라가 저절로 맞는다 — 받을 이벤트가 없으니 놓칠 것도 없다.
//    · 이탈→복귀가 저절로 맞는다 — RestoreVitals 가 장비 GE 를 다시 얹으면 그냥 다시 유도한다.
//    · 누수가 구조적으로 불가능하다. BGPlayerState.h 가 적어 둔 버그("배열만 비우고 GE 를
//      남기면 장비는 없는데 효과는 계속")를 이펙트에서 되풀이할 자리가 없다.
//
// ── 규칙에 관여하지 않는다 ────────────────────────────────────────────────────
//
//  아무것도 복제하지 않고 서버에 아무것도 알리지 않는다. 이 컴포넌트가 통째로 없어도
//  게임은 똑같이 돌아간다. 각 클라가 자기 화면에 로컬로 스폰할 뿐이다.
//
// ── 연출 이펙트와 섞지 말 것 ──────────────────────────────────────────────────
//
//  ⚠️ 여기서 ABGGameState::IsCinematicPlaying() 을 검사하게 되면 그 순간 분리가 깨진 것이다.
//     지속 이펙트는 연출과 무관하게 유지되고, 연출 이펙트는 연출이 끝나면 사라진다.
//     사망 중 오라를 끄는 것도 연출 단계가 아니라 bDeathShown(화면상 사망)으로 판단한다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGStatusFXComponent.generated.h"

class ABGPlayerState;
class ABGGameState;
class UNiagaraComponent;
struct FBGStatusFXRow;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGStatusFXComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGStatusFXComponent();

	/**
	 * 켜져 있어야 할 이펙트 집합을 복제 상태에서 다시 유도한다.
	 *
	 * 평소에는 알아서 불린다(장비·상태·사망 통지). 직접 부를 일은 디버깅이나
	 * 표를 런타임에 바꿔 끼웠을 때뿐이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|FX")
	void RefreshStatusFX();

	/** 지금 붙어 있는 이펙트 키 목록. 디버깅용. */
	UFUNCTION(BlueprintPure, Category = "BG|FX")
	TArray<FName> GetActiveFXKeys() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * 지금 켜져 있어야 할 키 집합을 만든다. 부수 효과 없음.
	 *
	 * 화면상 죽었으면(bDeathShown) 빈 집합이다 — 규칙상 사망(bIsDead)이 아니라 이쪽을 보는 이유는
	 * 그 둘이 일부러 갈라져 있기 때문이다. bIsDead 로 판단하면 죽는 일격을 맞는 순간
	 * 오라가 먼저 꺼지고, 그 뒤에 카드 연출이 재생된다.
	 */
	void BuildDesiredKeys(TSet<FName>& OutKeys) const;

	/** 표에서 Row 를 찾아 하나 붙인다. Row 가 없거나 Effect 가 비었으면 nullptr. */
	UNiagaraComponent* SpawnFXFor(FName Key);

	/** 소유 캐릭터의 PlayerState. 아직 안 붙었으면 nullptr. */
	ABGPlayerState* GetOwnerPlayerState() const;

	/** 누군가의 공개 정보가 바뀌었다. 내 좌석일 때만(또는 아직 좌석을 모를 때) 다시 유도한다. */
	void HandleSeatPublicInfoChanged(int32 ChangedSeat);

	/**
	 * PlayerState 가 이 폰보다 늦게 붙는 경우가 있다(특히 늦게 들어온 클라).
	 * 붙을 때까지만 돌고 스스로 멈춘다.
	 *
	 * 통지로 대신할 수 없는 이유: 장비도 상태도 없는 사람은 OnRep 이 아예 안 뜨고,
	 * 이미 장비가 있는 사람은 우리 폰이 생기기 **전에** 통지가 지나가 버린다.
	 * UBGSeatInfoDisplayComponent 가 "폰이 생겼다" 를 폴링으로 잡는 것과 같은 사정이다.
	 */
	void PollPlayerState();
	void BindPresentationRelease();

	/** 붙어 있는 이펙트. 키는 장비 CardId 또는 상태 태그 이름. */
	UPROPERTY()
	TMap<FName, TObjectPtr<UNiagaraComponent>> ActiveFX;

	FDelegateHandle PublicInfoHandle;
	FDelegateHandle PresentationReleaseHandle;
	TWeakObjectPtr<ABGGameState> BoundPresentationGameState;
	FTimerHandle    PlayerStateWaitTimer;

	/** 표가 없다는 경고를 한 번만 찍기 위한 표시. 매 갱신마다 찍으면 로그가 묻힌다. */
	bool bWarnedMissingTable = false;
};
