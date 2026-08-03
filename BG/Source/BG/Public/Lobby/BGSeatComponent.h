// BANG! 좌석 배정 — 서버 권위. 세션 참여 시 빈 자리를 주고, 판 시작 시 다시 랜덤으로 섞는다.
// 좌석은 거리 계산·대상 지정의 기준이라 소유자를 한 곳으로 모은다(게임모드에 로직을 얹지 않는다).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BGSeatComponent.generated.h"

class ABGGameMode;
class ABGPlayerState;

UCLASS(ClassGroup = (BG), meta = (BlueprintSpawnableComponent))
class BG_API UBGSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBGSeatComponent();

	// 현재 월드의 게임모드에 붙은 컴포넌트. 서버가 아니면 nullptr.
	static UBGSeatComponent* Find(const UWorld* World);

	// 서버 전용. 빈 좌석 중 가장 작은 번호를 준다. 이미 배정돼 있으면 그대로 둔다.
	// 자리가 없으면 INDEX_NONE (입장 거절은 UBGMatchStartComponent 소관이다).
	int32 AssignSeatOnJoin(APlayerController* NewPlayer);

	// 서버 전용. 퇴장한 플레이어의 좌석을 반납한다. 빈 번호는 메우지 않는다 —
	// 판 시작 시 RandomizeSeats 가 0..N-1 로 다시 채운다.
	void ReleaseSeat(AController* Exiting);

	// 서버 전용. 인원수만큼의 자리를 테이블에 고르게 잡고 그 안에서 사람을 섞는다(판 시작).
	// 배정 결과를 로그로 남긴다 — 재현·검증은 이 줄을 본다.
	void RandomizeSeats();

	// 이 좌석을 이미 누가 쓰고 있는가.
	UFUNCTION(BlueprintPure, Category = "BG|Seat")
	bool IsSeatTaken(int32 Seat) const;

	// 이 컨트롤러가 앉을 PlayerStart. 좌석이 없거나 레벨에 자리 액터가 없으면 nullptr
	// (그 경우 경고를 남긴다 — 호출자는 엔진 기본 지점으로 폴백하면 된다).
	AActor* ChooseStartFor(const AController* Player) const;

	// PlayerStartTag 접두사. 레벨 배치와 반드시 맞춰야 한다.
	static const TCHAR* SeatStartTagPrefix;

private:
	ABGGameMode* GetGameMode() const;

	// 접속 중인 BG 플레이어 상태를 모은다.
	void CollectPlayerStates(TArray<ABGPlayerState*>& OutStates) const;

	// 자리를 고르는 순서 — 짝수 자리를 먼저 채우고 홀수로 넘어간다(0,2,4,6,1,3,5).
	// 입장 배정과 판 시작 재배정이 같은 규칙을 쓴다.
	static void BuildSeatOrder(TArray<int32>& OutSeats);

	// 좌석 번호에 대응하는 PlayerStart. PlayerStartTag 가 "Seat0".."Seat6" 인 것을 찾는다.
	AActor* FindSeatStart(int32 Seat) const;

	// 폰을 자기 좌석 자리로 옮긴다(좌석이 바뀐 뒤 호출).
	void SnapPawnToSeat(ABGPlayerState* PS) const;
};
