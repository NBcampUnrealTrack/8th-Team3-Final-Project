// 코드로 만든 텍스트 위젯은 엔진 기본 폰트(Roboto)를 물고 나온다 — 기본 텍스트 스타일의 서체로 맞춘다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BGUIStyleSubsystem.generated.h"

class UCommonTextStyle;
class UUserWidget;
struct FSlateFontInfo;

UCLASS()
class BG_API UBGUIStyleSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 기본 텍스트 스타일(BGDataSettings.DefaultTextStyle)의 서체를 입힌다.
	 * 크기·외곽선은 호출하는 쪽 값을 그대로 둔다.
	 */
	static void ApplyDefaultTypeface(const UUserWidget* Owner, FSlateFontInfo& Font);

private:
	UPROPERTY()
	TSubclassOf<UCommonTextStyle> DefaultTextStyle;
};
