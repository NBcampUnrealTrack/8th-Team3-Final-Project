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

	/**
	 * 월드의 캠프파이어를 찾는다. 없으면 nullptr.
	 * 둘 이상 있으면 경고를 남기고 첫 번째를 쓴다 — 조용히 하나를 고르면
	 * "왜 저쪽 불 주위에 앉지" 를 한참 뒤에야 알게 된다.
	 */
	static ABGCampfire* Find(const UWorld* World);

	// ── 배치 설정 ────────────────────────────────────────────────

	/** 캠프파이어 중심에서 플레이어까지의 거리. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BG|Seating", meta = (ClampMin = "0.0"))
	float SeatRadius = 400.f;

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
	 * 총 SeatCount 명 중 Ordinal 번째 자리의 월드 트랜스폼. 순수 계산이라 클라에서도 안전하다.
	 *
	 * 회전은 항상 캠프파이어를 바라본다(요만 쓴다 — 피치·롤을 넣으면 캐릭터가 기운다).
	 * SeatCount 가 0 이하면 캠프파이어 자신의 트랜스폼을 돌려준다.
	 */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	FTransform GetSeatTransform(int32 Ordinal, int32 SeatCount) const;

	/** 인원수에 따른 좌석 간 각도(도). SeatCount 가 0 이하면 0. */
	UFUNCTION(BlueprintPure, Category = "BG|Seating")
	static float GetAngleStepDeg(int32 SeatCount) { return SeatCount > 0 ? 360.f / SeatCount : 0.f; }

	/**
	 * 좌석 번호로 트랜스폼을 얻는다. 현재 접속 인원을 기준으로 순번을 계산한다.
	 * 그 좌석이 비어 있으면 false.
	 *
	 * ⚠️ 돌려주는 위치는 **바닥의 한 점**이다. 폰을 놓을 때는 캡슐 반높이만큼 올려야 한다 —
	 *    직접 쓰지 말고 PlacePawn 을 쓸 것.
	 */
	UFUNCTION(BlueprintCallable, Category = "BG|Seating")
	bool GetTransformForSeat(int32 SeatIndex, FTransform& OutTransform) const;

	/**
	 * 폰 하나를 좌석 트랜스폼에 세운다. 폰을 자리에 놓는 **유일한 경로**다.
	 *
	 * ⚠️ 여기에 모은 이유: 좌석 트랜스폼은 바닥 좌표라 그대로 TeleportTo 하면 캡슐 절반이
	 *    지면에 파묻힌다. 그러면 FindTeleportSpot 이 자리를 찾지 못해 TeleportTo 가 false 를
	 *    반환하고 **액터는 제자리에 그대로 남는다** — 아무 오류 없이 조용히 실패한다.
	 *    캡슐 반높이 보정과 실패 로그를 이 함수 하나에만 두어 호출부마다 빠뜨리지 않게 한다.
	 */
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
	/** 좌석 번호를 오름차순으로 모은다. 비어 있는 번호는 빠지므로 결과는 촘촘하다. */
	void CollectOccupiedSeats(TArray<int32>& OutSeats) const;

	/** 캠프파이어 본체. 메시는 BP 에서 지정한다 — 여기서는 위치 기준일 뿐이다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BG|Campfire")
	TObjectPtr<UStaticMeshComponent> FireMesh;
};
