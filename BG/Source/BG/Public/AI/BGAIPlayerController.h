// BANG! 봇 컨트롤러 - PvE 에서 빈 좌석을 채운다. 연결 없이 서버가 직접 스폰한다.
// 카드를 고르는 판단은 여기 없다 - UBGAIBrainComponent 가 맡는다(미구현).
//
// BP 를 BP_BGPlayerController 의 자식으로 만들지 말 것 - 그 BP 의 그래프가
// WBP_BGHUD 를 만들어 화면 없는 봇이 HUD 를 띄운다.

#pragma once

#include "CoreMinimal.h"
#include "Core/BGPlayerController.h"
#include "BGAIPlayerController.generated.h"

UCLASS()
class BG_API ABGAIPlayerController : public ABGPlayerController
{
	GENERATED_BODY()

public:
	ABGAIPlayerController();
};
