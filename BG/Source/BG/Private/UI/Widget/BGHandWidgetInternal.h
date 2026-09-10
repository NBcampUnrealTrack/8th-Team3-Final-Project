// UBGHandWidget 의 .cpp 들이 나눠 쓰는 것.
//
// 파일을 책임별로 가르면서 한 파일 안 익명 네임스페이스에 있던 것을 여기로 옮겼다.
//
// ⚠️ Private 전용이다. 손패 바깥에서 include 하지 말 것.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

//	⚠️ STATIC 이 아니다. 번역 단위가 여럿이라 한 곳(BGHandWidget.cpp)에서만 정의한다.
DECLARE_LOG_CATEGORY_EXTERN(LogBGUI, Log, All);

namespace BGHand
{
	//	손패를 떠나는 카드. 손패 카드(0 ~ 장수)보다 확실히 크면 된다.
	constexpr int32 LeavingCardZOrder = 1000;

	//	덱에서 날아오는 카드. 이미 놓인 카드 위를 지나가야 궤적이 가리지 않는다.
	constexpr int32 DealingCardZOrder = 500;

	//	마우스가 올라간 카드. 부채꼴에서 겹친 이웃에 가리면 안 된다.
	constexpr int32 HoveredCardZOrder = 300;
}
