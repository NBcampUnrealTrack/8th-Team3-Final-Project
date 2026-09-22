#include "UI/BGUIStyleSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Card/BGDataSettings.h"
#include "CommonTextBlock.h"
#include "Engine/GameInstance.h"

void UBGUIStyleSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	//	게임 중에 디스크에서 읽지 않도록 시작할 때 한 번 로드해 붙잡아 둔다.
	DefaultTextStyle = GetDefault<UBGDataSettings>()->DefaultTextStyle.LoadSynchronous();
	ensureMsgf(DefaultTextStyle, TEXT("DefaultTextStyle 로드 실패 — BGDataSettings 에서 지정할 것"));
}

void UBGUIStyleSubsystem::ApplyDefaultTypeface(const UUserWidget* Owner, FSlateFontInfo& Font)
{
	check(Owner);

	//	디자이너 미리보기·에디터 월드에는 게임 인스턴스가 없다. 로드 실패는 Initialize 가 이미 알렸다.
	const UGameInstance* GameInstance = Owner->GetGameInstance();
	const UBGUIStyleSubsystem* Self = GameInstance ? GameInstance->GetSubsystem<UBGUIStyleSubsystem>() : nullptr;
	if (!Self || !Self->DefaultTextStyle)
	{
		return;
	}

	FSlateFontInfo StyleFont;
	GetDefault<UCommonTextStyle>(Self->DefaultTextStyle)->GetFont(StyleFont);
	Font.FontObject = StyleFont.FontObject;
	Font.TypefaceFontName = StyleFont.TypefaceFontName;
}
