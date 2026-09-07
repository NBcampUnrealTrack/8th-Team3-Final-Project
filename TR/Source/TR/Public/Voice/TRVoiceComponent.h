// TRVoiceComponent.h
// 무전 입력만 담당한다. 어떤 값을 쓸지는 UTRVoiceSubsystem 이 정한다.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "TRVoiceComponent.generated.h"

class APlayerController;

/**
 * 로컬 PlayerController 에 붙는 무전 입력 컴포넌트.
 * UTRVoiceSubsystem 이 컨트롤러가 바뀔 때마다 붙인다 — 컨트롤러 클래스는 건드리지 않는다.
 */
UCLASS(Config = Game, ClassGroup = (TR), meta = (BlueprintSpawnableComponent))
class TR_API UTRVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTRVoiceComponent();

	/**
	 * 소유 컨트롤러의 입력 컴포넌트에 키를 건다. 여러 번 불러도 한 번만 걸린다.
	 * 입력 컴포넌트가 아직 없으면 걸지 않고 false — 서브시스템이 다음 기회에 다시 부른다.
	 */
	bool BindKeys();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void StartPushToTalk();

	UFUNCTION(BlueprintCallable, Category = "TR|Voice")
	void StopPushToTalk();

	/** 오픈마이크로 바뀌면 눌린 상태와 무관하게 송신을 연다. 서브시스템이 부른다. */
	void ApplyTransmitMode(bool bPushToTalk);

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsPushToTalkHeld() const { return bPushToTalkHeld; }

	UFUNCTION(BlueprintPure, Category = "TR|Voice")
	bool IsBound() const { return bBound; }

	FKey GetPushToTalkKey() const { return PushToTalkKey; }

	FKey GetSettingsUIKey() const { return SettingsUIKey; }

protected:
	virtual void BeginPlay() override;

private:
	void ToggleSettingsUI();

	APlayerController* GetOwningController() const;

	/** 무전 키. DefaultGame.ini 의 [/Script/TR.TRVoiceComponent] 에서 빌드 없이 바꾼다. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "TR|Voice")
	FKey PushToTalkKey = EKeys::V;

	/** 임시 음성 설정 패널 토글 키. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "TR|Voice")
	FKey SettingsUIKey = EKeys::F9;

	bool bBound = false;

	bool bPushToTalkHeld = false;
};
