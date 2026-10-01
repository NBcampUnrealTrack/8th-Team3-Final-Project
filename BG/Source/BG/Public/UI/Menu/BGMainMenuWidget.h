// 메인메뉴 위젯 베이스. 세로 버튼 스택이고 PvP 를 누르면 방생성/방참가가 펼쳐진다.
// 세션 호출은 UBGGameInstance 가 갖는다 — 여기는 버튼을 그쪽으로 넘긴다.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BGMainMenuWidget.generated.h"

class UButton;
class UWidgetSwitcher;
class UBGGameInstance;
class UBGSystemMenuWidget;

UCLASS(Abstract)
class BG_API UBGMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	// BP 에서 같은 이름으로 배치해야 바인딩된다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_PvE;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_PvP;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Host;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Join;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Settings;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Quit;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

	// 설정 팝업. WBP_BGMainMenu 의 BG|UI 에서 WBP_BGSystemMenu 를 골라 둬야 버튼이 동작한다.
	UPROPERTY(EditDefaultsOnly, Category = "BG|UI")
	TSubclassOf<UBGSystemMenuWidget> SettingsWidgetClass;

	// 처음 누를 때 한 번 만들고 계속 재사용한다.
	UPROPERTY(Transient)
	TObjectPtr<UBGSystemMenuWidget> SettingsWidgetInstance;

	// 0 = 메인 페이지, 1 = 멀티플레이 페이지.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> Switcher_Menu;

private:
	UFUNCTION()
	void HandlePvE();

	UFUNCTION()
	void HandlePvP();

	UFUNCTION()
	void HandleHost();

	UFUNCTION()
	void HandleJoin();

	UFUNCTION()
	void HandleSettings();

	UFUNCTION()
	void HandleQuit();

	UFUNCTION()
	void HandleBack();

	// PvP 하위 항목(방생성/방참가) 접기·펼치기.
	void SetMultiplayerSubmenuVisible(bool bVisible);

	UBGGameInstance* GetBGGameInstance() const;
};
