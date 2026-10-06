// 팝업이 열릴 때 가운데에서 세로로 펼쳐지며 나타나는 연출.
// 도움말 패널이 첫 프레임에 줄바꿈 높이를 다시 재면서 우연히 보이던 모양을 일부러 만든 것이다.
//
// UMG 애니메이션이 아니라 코드로 둔 이유: 여러 WBP 에 같은 곡선을 손으로 맞춰 넣지 않기 위해서.
// 주인 위젯이 열 때 Start 를 부르고, NativeTick 에서 Tick 을 불러 준다.
//
// ⚠️ 대상의 렌더 크기와 RenderOpacity 를 덮어쓴다. 다른 코드가 같은 값을 만지는 위젯
//    (카드처럼 스스로 렌더 트랜스폼을 쓰는 위젯)에는 걸지 말고, 그 바깥 상자에 걸 것.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UWidget;

struct BG_API FBGPopupOpenAnim
{
	/** 첫 프레임부터 접힌 모양으로 세워 두고 펼치기 시작한다. 진행 중이면 처음부터 다시 한다. */
	void Start(UWidget* InTarget);

	void Tick(float DeltaTime);

private:
	TWeakObjectPtr<UWidget> Target;

	/** 음수면 쉬는 중. */
	float Elapsed = -1.f;
};
