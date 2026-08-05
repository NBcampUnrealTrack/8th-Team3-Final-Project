// BAAM 좌석 배치 콘솔 테스트 트리거. BGCardExec.cpp / BGSessionExec.cpp 와 같은 관례:
//   선언은 헤더에 항상 남기고(UFUNCTION(Exec)), 본문만 #if !UE_BUILD_SHIPPING 으로 감싼다.
//
// 호스트 클래스가 ABGGameMode 인 이유 (BGCardExec.cpp 머리말과 같은 판단):
//   폰을 옮기는 것은 서버 권위 조작이다(ArrangePlayers 는 HasAuthority 로 막혀 있다).
//   GameMode 에 두면 클라 창에서는 World->GetAuthGameMode() 가 null 이라 명령이 해석조차
//   되지 않고 "Command not recognized" 가 뜬다 — 서버 창에서 쳐야 한다는 사실이 즉시 드러난다.
//   PlayerController 에 뒀다면 클라에서 조용히 아무 일도 일어나지 않아 원인을 찾기 어렵다.

#include "Core/BGGameMode.h"

#include "Core/BGCampfire.h"
#include "Core/BGGameState.h"
#include "Core/BGPlayerState.h"
#include "Lobby/BGReadyComponent.h"
#include "Debug/BGCardLog.h"
#include "Engine/World.h"

void ABGGameMode::BG_ArrangeSeats(float Radius)
{
#if !UE_BUILD_SHIPPING
	//	게임모드가 캐시한 것을 쓴다 — 명령을 반복해서 쳐도 매번 액터를 순회하지 않는다.
	//	지역 변수명이 Fire 인 것은 멤버 Campfire 를 섀도잉하지 않기 위해서다(C4458 이 에러로 승격됨).
	ABGCampfire* Fire = GetCampfire();
	if (!Fire)
	{
		//	캠프파이어가 없으면 좌석 위치는 레벨의 PlayerStart(Seat0..)에서 나온다.
		//	"명령을 쳤는데 아무도 안 움직인다" 의 원인이 여기이므로 분명히 알린다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Exec] 레벨에 ABGCampfire 가 없습니다 — 배치할 기준점이 없습니다. "
			     "레벨에 캠프파이어를 배치하거나, PlayerStart(Seat0..) 방식을 쓰는 레벨인지 확인하세요."));
		return;
	}

	//	exec 는 생략된 뒤쪽 인자를 0 으로 넘긴다. C++ 기본값에 기대지 말 것
	//	(BG_DealHand 가 같은 이유로 Count <= 0 을 검사한다).
	if (Radius > 0.f)
	{
		UE_LOG(LogBGCard, Log, TEXT("[Exec] 반지름 %.0f → %.0f"), Fire->SeatRadius, Radius);
		Fire->SeatRadius = Radius;
	}

	const int32 SeatedCount = Fire->GetSeatedPlayerCount();
	if (SeatedCount == 0)
	{
		//	접속은 했지만 좌석 번호가 없는 상태다. ArrangePlayers 는 조용히 빠져나가므로
		//	여기서 짚어 주지 않으면 "반지름을 바꿨는데 왜 그대로지" 로 시간을 버린다.
		UE_LOG(LogBGCard, Warning,
			TEXT("[Exec] 좌석을 배정받은 플레이어가 없습니다 — UBGSeatComponent 가 좌석을 주기 전이거나 "
			     "접속자가 없습니다. 좌석 번호는 캠프파이어가 아니라 좌석 컴포넌트가 정합니다."));
		return;
	}

	UE_LOG(LogBGCard, Log,
		TEXT("===== BG_ArrangeSeats — %d명 / 반지름 %.0f / 간격 %.1f도 ====="),
		SeatedCount, Fire->SeatRadius, ABGCampfire::GetAngleStepDeg(SeatedCount));

	//	실제 배치는 캠프파이어가 한다 — 이 명령은 호출 시점만 대신 잡아 준다.
	//	폰이 아직 스폰되지 않은 좌석은 조용히 건너뛴다(ArrangePlayers 안에서 처리).
	Fire->ArrangePlayers();
#endif // !UE_BUILD_SHIPPING
}

void ABGGameMode::BG_ReadyAll()
{
#if !UE_BUILD_SHIPPING
	const ABGGameState* GS = GetGameState<ABGGameState>();
	if (!GS)
	{
		UE_LOG(LogBGCard, Warning, TEXT("[Exec] GameState 없음"));
		return;
	}

	int32 Count = 0;
	for (APlayerState* Base : GS->PlayerArray)
	{
		if (UBGReadyComponent* Ready = UBGReadyComponent::Find(Base))
		{
			Ready->SetReadyAuthoritative(true);
			++Count;
		}
	}
	UE_LOG(LogBGCard, Warning, TEXT("[Exec] BG_ReadyAll — %d명 준비 처리"), Count);
#endif // !UE_BUILD_SHIPPING
}
