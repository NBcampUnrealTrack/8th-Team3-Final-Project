// BAAM 캠프파이어 — 판의 중심이자 좌석 "위치" 의 기준점.
//
// 아트 기획서: "모닥불을 중앙에 두고 4~7인의 캐릭터가 둥글게 둘러서있습니다."
// 그 원을 레벨에 손으로 배치하는 대신 반지름과 인원수로 계산한다.
//
// ── 좌석 번호와 좌석 위치는 다른 것이다 ──────────────────────────
//   UBGSeatComponent — 누가 몇 번 좌석인가 (배정·재배정·랜덤화). 서버 권위.
//   ABGCampfire      — 몇 번 좌석이 어디에 서는가 (반지름·각도 계산).
//
//   섞으면 "자리를 옮겼는데 대상 지정이 어긋나는" 버그가 난다. 캠프파이어는
//   좌석 번호를 절대 바꾸지 않는다 — 이미 정해진 번호를 좌표로 옮기기만 한다.
//
// ── 인원수에 맞춰 각도를 계산한다 ────────────────────────────────
//   로비에서는 좌석 번호가 듬성듬성할 수 있다(UBGSeatComponent 는 0,2,4,6,1,3,5 순으로
//   채운다). 그래서 좌석 번호를 그대로 각도로 쓰면 3명일 때 0·2·4번이 원의 절반에만 몰린다.
//   대신 **채워진 좌석을 번호순으로 정렬한 뒤 그 순번(Ordinal)** 으로 각도를 나눈다.
//   3명이면 어떤 번호를 갖고 있든 120도씩 벌어진다.
//
// ── 불빛은 이 액터가 가진다 ──────────────────────────────────────
//   아트 기획서의 시간대는 노을 직후다. 태양은 지평선에 걸려 약하고, **실제로 캐릭터를
//   비추는 주광은 모닥불 하나뿐**이다. 그래서 불빛을 레벨에 따로 놓지 않고 여기 둔다 —
//   좌석 반지름을 아는 액터가 감쇠 반경도 정해야 둘이 어긋나지 않는다.
//
//   ⚠️ 순전히 보이는 것이다. 복제하지 않고 서버 권위와도 무관하다.
//      각 클라가 자기 화면에서 알아서 흔든다(ABGCinematicCamera 와 같은 성격).
//      데디케이티드 서버는 아무것도 그리지 않으므로 BeginPlay 에서 틱을 끈다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BGCampfire.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS()
class BG_API ABGCampfire : public AActor
{
	GENERATED_BODY()

public:
	ABGCampfire();

	static ABGCampfire* Find(const UWorld* World);

	// ── 배치 설정 ────────────────────────────────────────────────

	/** 캠프파이어 중심에서 플레이어까지의 거리. 레벨의 의자·PS_Seat* 반지름과 같아야 한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating", meta = (ClampMin = "0.0"))
	float SeatRadius = 500.f;

	/**
	 * 로비의 고정 자리 수(정원). 판 시작 전에는 이 수로 원을 나눈 자리에 선다 —
	 * 레벨의 PS_Seat0~6 이 그 좌표다. 판이 시작되면 참여 인원수로 다시 나눈다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating", meta = (ClampMin = "1"))
	int32 SeatSlotCount = 7;

	/** 1번 플레이어(정렬 순번 0)가 설 각도. 0도는 캠프파이어 기준 +X 방향이다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating")
	float FirstSeatAngleDeg = 0.f;

	/** 좌석 번호가 늘어나는 방향. 끄면 반시계(수학 기준), 켜면 시계 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating")
	bool bClockwise = false;

	/** 바닥 높이 보정. 캠프파이어 원점이 불 밑동이면 0 으로 둔다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating")
	float SeatHeightOffset = 0.f;

	// ── 계산 ─────────────────────────────────────────────────────

	/**
	 * SlotCount 등분한 원에서 Slot 번째 자리의 월드 트랜스폼. 순수 계산이라 클라에서도 안전하다.
	 * 회전은 항상 캠프파이어를 바라본다(요만 쓴다 — 피치·롤을 넣으면 캐릭터가 기운다).
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	FTransform GetSeatTransform(int32 Slot, int32 SlotCount) const;

	/** 자리 간 각도(도). SlotCount 가 0 이하면 0. */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	static float GetAngleStepDeg(int32 SlotCount) { return SlotCount > 0 ? 360.f / SlotCount : 0.f; }

	/**
	 * 좌석 번호의 트랜스폼. 좌석 번호가 곧 자리 번호다 — 인원수와 무관하게 같은 자리에 선다.
	 * 위치는 바닥의 한 점이므로 폰을 놓을 때는 PlacePawn 을 쓸 것(캡슐 반높이 보정).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Seating")
	bool GetTransformForSeat(int32 SeatIndex, FTransform& OutTransform) const;

	bool PlacePawn(APawn* Pawn, const FTransform& SeatTransform) const;

	// ── 배치 ─────────────────────────────────────────────────────

	/**
	 * 서버 전용. 접속한 플레이어의 폰을 좌석 번호순으로 원형 배치한다.
	 *
	 * 인원이 바뀔 때마다 부를 것 — 각도가 인원수로 결정되므로 한 명만 들어와도 전원이 움직인다.
	 * 폰이 아직 스폰되지 않은 좌석은 조용히 건너뛴다(다음 호출에서 잡힌다).
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Seating")
	void ArrangePlayers();

	/** 지금 좌석을 배정받은 플레이어 수. 각도 계산의 분모다. */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	int32 GetSeatedPlayerCount() const;

	// ── 불빛 ─────────────────────────────────────────────────────
	//
	//	값은 전부 여기서 노출한다 — 색감은 코드가 아니라 눈으로 잡는 것이라
	//	레벨에 놓고 슬라이더를 만지며 맞춰야 한다. 코드가 정하는 것은 "흔들림" 뿐이다.

	/** 불빛 자체를 끌지 여부. 끄면 라이트가 사라진다(태양광만으로 볼 때). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire")
	bool bFireLightEnabled = true;

	/**
	 * 흔들리지 않을 때의 기준 밝기(칸델라 — 프로젝트가 r.DefaultFeature.LightUnits=1).
	 * 노을 씬에서 태양이 2~8 lux 대이므로 불이 주광이 되려면 이보다 확실히 세야 한다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.0"))
	float FireIntensity = 25.f;

	/** 불꽃 색. 지평선 노을과 같은 계열로 두어야 씬이 한 팔레트로 묶인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire")
	FLinearColor FireColor = FLinearColor(1.f, 0.45f, 0.15f);

	/** 밝기를 흔들지 여부. 끄면 FireIntensity 로 고정된다 — 값을 잡을 때 꺼두면 편하다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire")
	bool bFlicker = true;

	/** 흔들리는 폭. 0.25 면 기준의 75%~125% 사이를 오간다. 0.4 를 넘기면 명멸이 거슬린다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FlickerIntensityVariance = 0.25f;

	/** 흔들리는 속도. 크면 촛불처럼 파닥이고, 작으면 큰 화톳불처럼 느리게 일렁인다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.0"))
	float FlickerSpeed = 7.f;

	/**
	 * 광원이 좌우로 흔들리는 폭(cm).
	 *
	 * 밝기보다 이쪽이 중요하다 — 밝기만 흔들면 씬 전체가 균일하게 명멸할 뿐이고,
	 * 둘러앉은 7명의 그림자가 살아 움직이는 것은 광원이 실제로 움직일 때뿐이다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.0"))
	float FlickerJitter = 4.f;

	/**
	 * 광원을 실제로 옮기는 최소 이동량(cm). 이보다 덜 움직인 프레임은 그냥 넘어간다.
	 *
	 * ⚠️ 성능 손잡이다. 그림자를 드리우는 Movable 라이트는 **움직인 프레임마다** 감쇠 반경
	 *    안의 동적 그림자를 다시 그린다 — 7명이 둘러앉은 이 씬에서는 그게 매 프레임이 된다.
	 *    밝기 흔들림은 이 필터와 무관하게 매 프레임 그대로 간다(그림자를 다시 그리지 않는다).
	 *
	 *    FlickerJitter 대비 너무 크게 잡으면 광원이 뚝뚝 끊겨 움직여 그림자가 튄다.
	 *    1/8 ~ 1/4 쯤이 적당하다. 0 이면 필터를 끄고 매 프레임 옮긴다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.0"))
	float FlickerJitterMinStep = 0.75f;

	/**
	 * 불 밑동에서 광원까지의 높이(cm). 불꽃 위쪽에 두면 그림자가 바깥으로 길게 눕는다.
	 * 너무 낮추면(불꽃 중심 이하) 그림자가 거의 수평으로 뻗어 좌석 뒤를 다 덮는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire")
	float LightHeightOffset = 100.f;

	/**
	 * 감쇠 반경을 SeatRadius 로부터 계산할지. 끄면 컴포넌트에 직접 넣은 값을 쓴다.
	 * 켜 두면 좌석 반지름을 바꿔도 불빛이 따라와서 둘이 어긋나지 않는다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire")
	bool bAutoAttenuationRadius = true;

	/** 감쇠 반경 = SeatRadius * 이 값. 1.6 이면 좌석 원 바깥까지 살짝 넘어간다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Fire", meta = (ClampMin = "0.1", EditCondition = "bAutoAttenuationRadius"))
	float AttenuationRadiusScale = 1.6f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	void CollectOccupiedSeats(TArray<int32>& OutSeats) const;

	/** 흔들림과 무관한 값(색·반경·기준 밝기)을 라이트에 밀어 넣는다. 에디터에서도 돈다. */
	void ApplyFireLightSettings();

	/** 매 프레임 밝기와 위치를 흔든다. bFlicker 가 꺼져 있으면 기준값으로 되돌린다. */
	void UpdateFireFlicker(float DeltaSeconds);

	/**
	 * 광원을 옮긴다. FlickerJitterMinStep 보다 덜 움직였으면 건너뛴다.
	 * 높이(LightHeightOffset)를 에디터에서 바꾼 경우도 같은 비교에 걸려 반영된다.
	 */
	void MoveFireLight(const FVector& RelativeLocation);

	/** 캠프파이어 본체. 메시는 BP 에서 지정한다 — 여기서는 위치 기준일 뿐이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<UStaticMeshComponent> FireMesh;

	/** 씬의 주광. 매 프레임 움직이므로 Movable 이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Fire")
	TObjectPtr<UPointLightComponent> FireLight;

private:
	/** 노이즈를 읽는 위치. FlickerSpeed 를 곱한 뒤 누적하므로 속도를 바꿔도 튀지 않는다. */
	float FlickerTime = 0.f;

	/**
	 * 라이트에 마지막으로 **실제로 밀어 넣은** 상대 위치. 임계값 비교의 기준이다.
	 * 목표 위치가 아니라 적용된 위치를 들고 있어야, 건너뛴 프레임이 쌓여도 편차가 커지지 않는다.
	 */
	FVector AppliedLightOffset = FVector::ZeroVector;

	/** 아직 한 번도 안 밀어 넣었다. 첫 프레임은 임계값과 무관하게 적용한다. */
	bool bLightOffsetApplied = false;
};
