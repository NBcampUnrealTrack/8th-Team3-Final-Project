// 코드로 만든 텍스트 위젯은 엔진 기본 폰트(Roboto)를 물고 나온다 — WBP 쪽과 같은 폰트로 맞춘다.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "UObject/ConstructorHelpers.h"

namespace BGUIFont
{
	//	⚠️ 생성자에서만 부른다. CDO 를 만들 때 한 번 잡아 두어 게임 중에 디스크에서 읽지 않게 한다.
	inline UFont* FindDefault()
	{
		static ConstructorHelpers::FObjectFinder<UFont> Finder(TEXT("/Game/UI/Font/HeirofLightRegular_Font.HeirofLightRegular_Font"));
		return Finder.Object;
	}

	inline void Apply(FSlateFontInfo& Font, UFont* FontObject)
	{
		if (!ensureMsgf(FontObject, TEXT("UI 폰트가 비어 있다 — 위젯 기본값의 TextFont 를 확인할 것")))
		{
			return;
		}
		Font.FontObject = FontObject;
		Font.TypefaceFontName = TEXT("Default");
	}
}
