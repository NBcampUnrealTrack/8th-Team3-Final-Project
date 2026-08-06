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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BGCampfire.generated.h"

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

protected:
	void CollectOccupiedSeats(TArray<int32>& OutSeats) const;

	/** 캠프파이어 본체. 메시는 BP 에서 지정한다 — 여기서는 위치 기준일 뿐이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<UStaticMeshComponent> FireMesh;
};
