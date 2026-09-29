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
#include "UI/Presentation/BGPresentationTypes.h"   // FBGPresentationRequest — UFUNCTION 인자라 완전한 타입이 필요하다
#include "Core/BGIntroStage.h"                     // EBGIntroStage
#include "BGSeatInfoDisplayComponent.generated.h"

class ABGCharacter;
class ABGPlayerController;
class UWidgetComponent;
enum class EBGCinematicStage : uint8;

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

	// 상세가 그려질 픽셀 크기. 네 줄(직업·장비·손패·역할)이 들어갈 만큼만 잡는다.
	// 한 줄 46px(아이콘 32 + 위아래 여백 7) + 줄 간격 2 → 4줄 192. 모자라면 아래가 잘린다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	FVector2D DetailDrawSize = FVector2D(180.f, 200.f);

	/**
	 * 상세와 이름표 사이 틈(픽셀). 음수면 그만큼 내려와 이름표와 겹친다.
	 * 월드 거리가 아니라 화면 픽셀이라 거리와 무관하게 일정하다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float DetailGap = -100.f;

	/** 상세를 가로로 밀 픽셀. 양수면 오른쪽. DetailGap 과 같은 원리로 거리와 무관하다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float DetailScreenOffsetX = 100.f;

	/** 캡슐 꼭대기에서 이만큼 더 올린다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float HeightAboveCapsule = 40.f;

	// 이름표가 그려질 픽셀 크기. 배경은 내용 크기에 맞춰지므로 이 값은 "넘지 않을 상한" 이다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	FVector2D WidgetDrawSize = FVector2D(260.f, 56.f);

	/** 이름표를 가로로 밀 픽셀. 음수면 왼쪽. DetailScreenOffsetX 와 같은 원리다. */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo")
	float PlateScreenOffsetX = -80.f;

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

	// ── 카드 연출 중 이름표 ──
	//
	//  HUD 와 함께 카드 연출 시작에 걷고, 피격 연출(Secondary)에서 다시 띄운다.
	//  바뀐 체력(피해·회복)은 연출에 맞춰서야 보여준다 — 규칙상 효과는 연출 전에 이미 적용돼 있다.

	/** 공격 연출이 시작될 때 이름표가 사라지는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.0"))
	float PlateFadeOutSeconds = 0.25f;

	/** 피격 연출이 시작될 때 이름표가 돌아오는 시간(초). */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.0"))
	float PlateFadeInSeconds = 0.3f;

	/**
	 * 피격 연출이 시작되고 이만큼 뒤에 하트를 깎는다.
	 * 이름표가 다 떠오르기 전에 깎이면 줄어드는 순간을 못 본다 — PlateFadeInSeconds 이상으로 둘 것.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.0"))
	float HealthRevealDelay = 0.45f;

	/**
	 * 내 하트(턴 패널)는 연출이 끝나고 이만큼 뒤에 깎는다.
	 * ⚠️ WBP_BGHUD 가 다시 떠오르는 애니메이션 길이 이상으로 둘 것 — 짧으면 HUD 가 반쯤 투명할 때 깎인다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.0"))
	float LocalHealthRevealDelay = 0.4f;

	/**
	 * 체력 변화가 연출 없이 도착했을 때, 연출이 뒤따라오는지 기다려 보는 시간(초).
	 *
	 * ⚠️ 체력(폰의 ASC)과 연출 상태(GameState)는 다른 액터로 복제돼 도착 순서가 보장되지 않는다.
	 *    이 유예가 없으면 체력이 한 발 먼저 온 클라에서는 하트가 연출 전에 깎여 버린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.0"))
	float HealthHoldGraceSeconds = 0.3f;

	/**
	 * 체력 표시를 붙잡아 둘 수 있는 상한(초). 연출이 비정상으로 끊겨도 표시가 영영 멈추지 않게 한다.
	 * ⚠️ 가장 긴 정상 흐름(판정 UI → 공격 → 피격 → 사망 → 역할 공개, 약 15초)보다 넉넉해야 한다 —
	 *    짧으면 내 하트가 HUD 가 숨은 동안 풀려 버린다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "BG|SeatInfo|Cinematic", meta = (ClampMin = "0.1"))
	float MaxHealthHoldSeconds = 25.f;

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

	/** 체력이 바뀌었다. 좌석마다 그 폰의 ASC 에 걸어 둔다. 피해든 회복이든 연출에 맞춰 드러나도록 표시를 붙잡는다. */
	void HandleHealthChanged(const struct FOnAttributeChangeData& Data, int32 SeatIndex);

	/** 최대 체력이 바뀌었다(직업 배정). 연출과 무관하므로 곧바로 반영한다. */
	void HandleMaxHealthChanged(const struct FOnAttributeChangeData& Data);

	/** 연출 단계가 바뀌었다. 피격(Secondary)에 닿으면 이름표를 다시 띄운다. */
	void HandleCinematicStageChanged(EBGCinematicStage Stage, ABGCharacter* Subject);

	/** 도입 연출 단계가 바뀌었다. 비출 좌석 하나만 남기고 나머지를 걷는다. */
	void HandleIntroStageChanged(EBGIntroStage Stage);

	/** 이 좌석의 이름표가 지금 떠 있어야 하는가(0 또는 1). */
	float GetTargetPlateOpacity(int32 SeatIndex) const;

	/** 카드 연출이 시작됐다 — HUD 가 걷히는 신호와 같다. 이름표도 걷는다. */
	UFUNCTION()
	void HandlePresentationStarted(const FBGPresentationRequest& Request);

	/** 카드 연출이 끝났다. 피격 단계가 없던 카드는 여기서 이름표가 돌아온다. */
	UFUNCTION()
	void HandlePresentationFinished(const FBGPresentationRequest& Request);

	/** 지금 상태로 이름표를 걷을지 정해 페이드 방향을 바꾼다. */
	void UpdatePlateVisibility();

	/** 붙잡아 둔 체력을 풀 때가 됐는지 본다. 표시가 바뀌었으면(다시 그렸으면) true. */
	bool TickHealthHold();

	/** 이 화면 주인의 좌석. 아직 배정 전이면 INDEX_NONE. */
	int32 GetLocalSeatIndex() const;

	void ReleaseHeldHealth();

	/** 이름표 투명도를 목표로 한 프레임 옮긴다. */
	void TickPlateFade(float DeltaTime);

	/** 투명도를 이름표 하나에 입힌다. 0 이면 컴포넌트째 숨긴다. */
	void ApplyPlateOpacity(UWidgetComponent* Widget, float Opacity) const;

	/** 월드 시간(초). 복제와 무관한 로컬 시계다 — 이 컴포넌트의 타이밍은 전부 화면 쪽 사정이다. */
	double GetNow() const;

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

	//	폰 → 좌석. 이탈로 컨트롤러가 떨어지면 폰에서 PlayerState 를 못 읽으므로,
	//	미리 기억해 둔 좌석으로 되찾아 이름표를 계속 띄운다.
	TMap<TWeakObjectPtr<ABGCharacter>, int32> SeatByCharacter;

	//	체력 통지를 걸어 둔 ASC. 폰이 바뀌면 여기서 떼고 다시 건다.
	TMap<int32, TWeakObjectPtr<class UAbilitySystemComponent>> BoundHealthASCs;

	//	구독해 둔 GameState. 중복 구독을 막는다.
	TWeakObjectPtr<class ABGGameState> BoundGameState;

	FDelegateHandle PublicInfoHandle;
	FDelegateHandle DisconnectHandle;
	FDelegateHandle CinematicStageHandle;
	FDelegateHandle IntroStageHandle;

	//	좌석 → 지금 보여주고 있는(바뀌기 전) 체력. 여기 있는 좌석은 실제 값 대신 이 값을 그린다.
	TMap<int32, int32> HeldHealthBySeat;

	//	붙잡기 시작한 시각. 상한 판정에 쓴다.
	double HealthHoldStartTime = 0.0;

	//	유예의 기준점 — 마지막 체력 변화 또는 마지막으로 연출 중이던 시각 중 늦은 쪽.
	double HoldGraceAnchorTime = 0.0;

	//	피격 연출이 시작된 시각. HealthRevealDelay 를 여기서부터 잰다.
	double SecondaryStageStartTime = 0.0;

	//	카드 연출이 끝난(HUD 가 돌아오기 시작한) 시각. LocalHealthRevealDelay 를 여기서부터 잰다.
	double PresentationFinishedTime = 0.0;

	//	구독해 둔 연출 디렉터. HUD 가 걷히는 신호를 여기서 받는다.
	TWeakObjectPtr<class UBGPresentationDirector> BoundDirector;

	//	카드 연출이 도는 중인가 / 이번 연출이 피격에 닿았는가 / 공격 연출(Primary) 중인가.
	bool bPresentationActive = false;
	bool bHitReached = false;
	bool bPrimaryStage = false;

	//	좌석 → 지금 화면에 입혀 둔 이름표 투명도. 도입 연출은 좌석마다 답이 달라 하나로 못 둔다.
	TMap<int32, float> PlateOpacityBySeat;

	//	카드 연출로 전원을 걷어 둔 상태인가.
	bool bPlatesHidden = false;

	//	도입 연출 중인가 / 그중 비추고 있는 좌석. INDEX_NONE 이면 아무도 안 비춘다.
	bool bIntroActive = false;
	int32 IntroFocusSeat = INDEX_NONE;

	//	좌석 배정을 기다리는 타이머. 배정되면 표시를 켜고 계속 감시한다(퇴장 대비).
	FTimerHandle SeatWaitTimer;

	int32 HoveredSeat = INDEX_NONE;

	bool bDisplayEnabled = false;

	//	위젯 클래스 없음 경고를 한 번만 찍기 위한 표시.
	bool bWarnedMissingInfoClass = false;

	float TimeSinceStructureCheck = 0.f;
};
