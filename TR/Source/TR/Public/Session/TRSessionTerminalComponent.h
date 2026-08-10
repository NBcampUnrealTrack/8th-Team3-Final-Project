// TR 세션 단말 — 이 컴포넌트가 붙은 구조물을 조작하면 세션 메뉴가 열린다.
//
// 특정 액터 클래스에 묶지 않는다. 기차의 어떤 구조물이든 이 컴포넌트만 얹으면 단말이 된다.
// 문구와 사용 가능 여부는 배치한 인스턴스마다 따로 조정할 수 있다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TRSessionTerminalComponent.generated.h"

UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent),
	HideCategories = (Tags, ComponentTick, ComponentReplication, Activation, Cooking, AssetUserData, Collision))
class TR_API UTRSessionTerminalComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRSessionTerminalComponent();

	/** 지금 이 단말을 쓸 수 있는가. 연출·진행 상황에 따라 껐다 켤 수 있다. */
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	bool CanUse() const { return bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }

	/** 조준했을 때 띄울 안내 문구. */
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	FText GetPromptText() const { return PromptText; }

protected:
	// 배치한 인스턴스마다 따로 켜고 끌 수 있다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	FText PromptText = NSLOCTEXT("TR", "SessionTerminalPrompt", "무전기 사용 — 방 만들기 / 참가");
};
