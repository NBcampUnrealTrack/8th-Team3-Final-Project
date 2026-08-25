#include "AI/BGAIPlayerController.h"

ABGAIPlayerController::ABGAIPlayerController()
{
	//	화면도 입력도 없다. 갱신 간격 0 이면 BeginPlay 가 UI 타이머를 걸지 않는다.
	bShowMouseCursor = false;
	SeatBoardRefreshInterval = 0.f;
}
