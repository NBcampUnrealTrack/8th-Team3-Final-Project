// TRPopupStack.h
//
// 로컬 플레이어에게 떠 있는 팝업 수를 센다.
// HUD 가 아니라 LocalPlayer 에 붙는 이유 — 세션 메뉴는 HUD 가 ATRPlayerHUD 가 아닌 맵(로비 등)
// 에서도 뜨고, 그런 곳에서도 팝업 중첩은 똑같이 일어난다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "TRPopupStack.generated.h"

class APlayerController;

UCLASS()
class TR_API UTRPopupStack : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	// 원격 클라이언트의 폰을 들고 있는 서버 쪽 컨트롤러에는 LocalPlayer 가 없어 nullptr 이 나온다.
	static UTRPopupStack* Get(const APlayerController* PC);

	void Push() { ++OpenCount; }

	// 마지막 팝업이 닫혔으면 true.
	bool Pop()
	{
		OpenCount = FMath::Max(0, OpenCount - 1);
		return OpenCount == 0;
	}

	// LocalPlayer 는 맵을 넘어가도 살아남는다. 트래블 중 위젯이 NativeDestruct 없이 사라지면
	// 수가 남으므로, 새 맵에서 HUD 가 시작할 때 한 번 털어 준다.
	void Reset() { OpenCount = 0; }

private:
	int32 OpenCount = 0;
};
