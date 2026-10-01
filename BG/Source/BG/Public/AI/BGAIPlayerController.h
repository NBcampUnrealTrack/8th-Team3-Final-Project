// BANG! 봇 컨트롤러 - PvE 에서 빈 좌석을 채운다. 연결 없이 서버가 직접 스폰한다.
// 카드를 고르는 판단은 UBGAIBrainComponent 가 맡는다.
//
// BP 를 BP_BGPlayerController 의 자식으로 만들지 말 것 - 그 BP 의 그래프가
// WBP_BGHUD 를 만들어 화면 없는 봇이 HUD 를 띄운다.

#pragma once

#include "CoreMinimal.h"
#include "Core/BGPlayerController.h"
#include "BGAIPlayerController.generated.h"

class UBGAIBrainComponent;

UCLASS()
class BG_API ABGAIPlayerController : public ABGPlayerController
{
	GENERATED_BODY()

public:
	ABGAIPlayerController();

	//	화면이 없다. Standalone 에서 IsLocalController 가 true 여도 UI 는 띄우지 않는다.
	virtual bool HasLocalViewer() const override { return false; }

private:
	// 카드 사용 판단은 전부 이 컴포넌트가 맡는다 - PvE 전용 클래스라 여기 붙여도
	// PvP 쪽에는 영향이 없다.
	UPROPERTY(VisibleAnywhere, Category = "BG|AI")
	TObjectPtr<UBGAIBrainComponent> AIBrain;
};
