// BANG! 머리 위 정보 표시 — 판이 도는 동안 모든 캐릭터 위에 좌석 정보를 띄운다.
// 마우스를 올린 하나만 상세로 넓어지고 나머지는 간략으로 남는다.
//
// 보는 사람에게만 있으면 되는 것이라 캐릭터가 아니라 PlayerController 에 붙는다.
// 그래서 ABGCharacter 를 건드리지 않고, 위젯도 복제되지 않는다.
//
// 본인 것은 머리 위에 띄우지 않고 HUD 의 턴 패널(좌하단)에 밀어넣는다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Widget/BGSeatDetailWidget.h"
#include "UI/Widget/BGSeatInfoWidget.h"
#include "BGSeatInfoDisplayComponent.generated.h"

class ABGCharacter;
class ABGPlayerController;
class UWidgetComponent;

/** 커서가 올라간 좌석이 바뀌었다. 없으면 INDEX_NONE. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBGHoveredSeatChanged, int32, SeatIndex);

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGSeatInfoDisplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGSeatInfoDisplayComponent();

	/** 커서가 올라간 좌석. 아무 데도 안 올라가 있으면 INDEX_NONE. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	int32 GetHoveredSeat() const { return HoveredSeat; }

	/** 커서가 올라간 캐릭터. 조준 쪽에서 액터가 필요할 때 쓴다. */
	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	ABGCharacter* GetHoveredCharacter() const;

	UPROPERTY(BlueprintAssignable, Category = "BG|SeatInfo")
	FOnBGHoveredSeatChanged OnHoveredSeatChanged;

	/** 표시를 켜고 끈다. 판이 아닐 때는 꺼 둔다 — 로비에서까지 뜰 이유가 없다. */
	UFUNCTION(BlueprintCallable, Category = "BG|SeatInfo")
	void SetDisplayEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "BG|SeatInfo")
	bool IsDisplayEnabled() const { return bDisplayEnabled; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** 머리 위에 얹을 이름표. 지정하지 않으면 아무것도 뜨지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	TSubclassOf<UBGSeatInfoWidget> InfoWidgetClass;

	/** 커서를 올렸을 때 그 캐릭터 위에 열리는 상세 패널. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	TSubclassOf<UBGSeatDetailWidget> DetailWidgetClass;

	// 상세가 그려질 픽셀 크기. 네 줄(직업·손패·역할·장비)이 들어갈 만큼만 잡는다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	FVector2D DetailDrawSize = FVector2D(300.f, 140.f);

	/** 상세와 이름표 사이 틈(픽셀). 월드 거리가 아니라 화면 픽셀이라 거리와 무관하게 일정하다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float DetailGap = 8.f;

	/** 캡슐 꼭대기에서 이만큼 더 올린다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float HeightAboveCapsule = 40.f;

	// 이름표가 그려질 픽셀 크기. 배경은 내용 크기에 맞춰지므로 이 값은 "넘지 않을 상한" 이다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	FVector2D WidgetDrawSize = FVector2D(260.f, 56.f);

	// 켜면 월드 평면에 붙어 원근을 받고, 끄면 항상 화면을 마주 본다.
	// 기본은 화면 방향이다 — 원탁 반대편 이름표가 뒤집혀 보이는 것을 막으려는 것이다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	bool bDrawInWorldSpace = false;

	// 이 거리를 넘으면 위젯을 그리지 않는다. 원탁 하나짜리 판이라 넉넉히 잡아도 된다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float MaxDrawDistance = 4000.f;

	/** 커서 트레이스 사거리. 원탁 지름보다 넉넉하면 된다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float HoverTraceDistance = 10000.f;

	// 위젯을 새로 붙일 좌석이 생겼는지 확인하는 주기(초).
	//
	// 값 갱신은 전부 델리게이트로 받는다. 다만 "이 좌석에 폰이 생겼다" 를 알려주는 통지가
	// 없어서, 늦게 도착한 폰을 잡으려면 이 확인만은 주기적으로 돌아야 한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float StructureCheckInterval = 1.f;

private:
	/** 좌석 목록에 맞춰 위젯을 만들고 없어진 좌석 것은 지운다. */
	void SyncWidgets();

	/** 내 좌석이 배정됐는지 보고 표시를 켜고 끈다. */
	void PollSeatAssigned();

	// ── 통지 구독 ──

	void BindDelegates();
	void UnbindDelegates();

	/** 페이즈·현재 좌석·판 진행 여부가 바뀌었다. */
	UFUNCTION()
	void HandleTurnStateChanged();

	/** 누군가의 손패 장수나 장비가 바뀌었다. */
	void HandleSeatPublicInfoChanged(int32 SeatIndex);

	/** 누군가의 이탈 표시가 바뀌었다. */
	void HandleDisconnectChanged(int32 SeatIndex, bool bDisconnected);

	/** 체력이 바뀌었다. 좌석마다 그 폰의 ASC 에 걸어 둔다. */
	void HandleHealthChanged(const struct FOnAttributeChangeData& Data);

	/** 이 좌석 폰의 체력 통지를 구독한다. 이미 걸려 있으면 아무것도 하지 않는다. */
	void BindHealthDelegate(class ABGCharacter* Character, int32 SeatIndex);

	/** 모든 이름표에 최신 FBGSeatView 를 밀어넣고, 열려 있는 상세 패널도 갱신한다. */
	void RefreshWidgets();

	/** 상세 패널을 연다. 대상이 화면 어느 쪽에 있는지에 따라 좌/우로 붙인다. */
	void ShowDetailFor(int32 SeatIndex);

	void HideDetail();

	/** 커서 아래 캐릭터를 찾아 HoveredSeat 를 갱신한다. 바뀌면 통지한다. */
	void UpdateHover();

	/** 좌석 하나에 이름표와 상세를 붙인다. 이미 있으면 그것을 돌려준다. */
	UWidgetComponent* EnsureWidgetFor(ABGCharacter* Character, int32 SeatIndex);

	/** 캐릭터에 붙는 위젯 컴포넌트 하나를 만든다. 이름표와 상세가 같은 설정을 쓴다. */
	UWidgetComponent* MakeWidgetOn(ABGCharacter* Character, TSubclassOf<class UUserWidget> WidgetClass,
		const FVector2D& DrawSize, float HeightZ, const FVector2D& Pivot);

	void DestroyAllWidgets();

	ABGPlayerController* GetOwningController() const;

	//	좌석 번호 → 그 좌석 캐릭터에 붙여 둔 위젯 컴포넌트.
	//	캐릭터가 사라지면(사망·이탈) 다음 SyncWidgets 에서 정리된다.
	UPROPERTY()
	TMap<int32, TObjectPtr<UWidgetComponent>> WidgetsBySeat;

	//	좌석마다 하나씩. 평소에는 전부 숨어 있고 커서가 올라간 하나만 켜진다.
	UPROPERTY()
	TMap<int32, TObjectPtr<UWidgetComponent>> DetailsBySeat;

	//	지금 열려 있는 상세의 좌석. 없으면 INDEX_NONE.
	int32 VisibleDetailSeat = INDEX_NONE;

	//	체력 통지를 걸어 둔 ASC. 폰이 바뀌면 여기서 떼고 다시 건다.
	TMap<int32, TWeakObjectPtr<class UAbilitySystemComponent>> BoundHealthASCs;

	//	구독해 둔 GameState. 중복 구독을 막는다.
	TWeakObjectPtr<class ABGGameState> BoundGameState;

	FDelegateHandle PublicInfoHandle;
	FDelegateHandle DisconnectHandle;

	//	좌석 배정을 기다리는 타이머. 배정되면 표시를 켜고 계속 감시한다(퇴장 대비).
	FTimerHandle SeatWaitTimer;

	int32 HoveredSeat = INDEX_NONE;

	bool bDisplayEnabled = false;

	//	위젯 클래스 없음 경고를 한 번만 찍기 위한 표시.
	bool bWarnedMissingInfoClass = false;

	float TimeSinceStructureCheck = 0.f;
};
