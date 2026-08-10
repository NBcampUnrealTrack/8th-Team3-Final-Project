// TR 세션 단말 조작 — 조준한 구조물이 단말이면 세션 메뉴를 연다.
//
// 메뉴는 보는 사람 화면에만 있으면 되므로 서버로 보내지 않는다. 그래서 기존
// ATRPlayerCharacter::Interact(서버 권한) 와 경로가 갈린다. 여기는 로컬 전용이다.
//
// 플레이어 캐릭터나 컨트롤러에 붙인다. 조준 판정은 UTRInteractionComponent 것을 그대로 쓴다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "TRSessionInteractionComponent.generated.h"

class UInputAction;
class UTRSessionTerminalComponent;

/** 조준한 단말이 바뀌었다. 없으면 nullptr. 안내 문구를 띄우는 데 쓴다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTRSessionTerminalFocused,
	UTRSessionTerminalComponent*, Terminal);

UCLASS(ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTRSessionInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRSessionInteractionComponent();

	/** 지금 조준하고 있는 단말. 없으면 nullptr. */
	UFUNCTION(BlueprintPure, Category = "TR|Session")
	UTRSessionTerminalComponent* GetFocusedTerminal() const { return FocusedTerminal.Get(); }

	UPROPERTY(BlueprintAssignable, Category = "TR|Session")
	FOnTRSessionTerminalFocused OnTerminalFocused;

	/** 단말을 보고 있으면 메뉴를 연다. 안 보고 있으면 아무 일도 없다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void TryOpenFromTerminal();

	/** 단말과 무관하게 메뉴를 여닫는다. 디버그 키가 이걸 부른다. */
	UFUNCTION(BlueprintCallable, Category = "TR|Session")
	void ToggleMenu();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * 단말을 조작할 입력. 기존 상호작용 키(IA_Interact)를 그대로 넣어도 되고
	 * 따로 만들어 넣어도 된다. 비워 두면 조작 키 없이 디버그 키로만 열린다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	TObjectPtr<UInputAction> IA_UseTerminal;

	/** 단말 없이도 메뉴를 여는 디버그 키. 배포 전에 비우면 된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session|Debug")
	FKey DebugToggleKey = EKeys::F9;

	/** 조준 대상을 다시 보는 주기(초). 매 프레임 볼 필요는 없다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TR|Session")
	float FocusCheckInterval = 0.15f;

private:
	/** 조준 대상에서 단말 컴포넌트를 찾아 FocusedTerminal 을 갱신한다. 바뀌면 통지한다. */
	void UpdateFocus();

	/** 입력을 건다. 컨트롤러나 인풋 컴포넌트가 늦게 오면 다음 틱에 다시 시도한다. */
	bool TryBindInput();

	class APlayerController* GetOwningController() const;

	TWeakObjectPtr<UTRSessionTerminalComponent> FocusedTerminal;

	bool bInputBound = false;

	float TimeSinceFocusCheck = 0.f;
};
