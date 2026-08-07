// 조준 중에 클릭을 통과시켜야 하는 게임플레이 HUD 위젯을 알려 준다.
// 조준 컴포넌트가 어떤 위젯이 있는지 몰라도 되게 하려는 것이다 — 위젯 참조는 HUD 를
// 들고 있는 쪽(PlayerController)에만 둔다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BGGameplayHudOwner.generated.h"

class UWidget;

UINTERFACE(MinimalAPI)
class UBGGameplayHudOwner : public UInterface
{
	GENERATED_BODY()
};

class BG_API IBGGameplayHudOwner
{
	GENERATED_BODY()

public:
	/** 조준 중 클릭을 통과시킬 위젯들. null 이 섞여 있어도 된다. */
	virtual void CollectGameplayHudWidgets(TArray<UWidget*>& OutWidgets) const = 0;
};
