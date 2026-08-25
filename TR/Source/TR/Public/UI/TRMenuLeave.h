// TRMenuLeave.h

#pragma once

#include "CoreMinimal.h"

namespace TRMenuLeave
{
	// 세션을 정리하고 메인메뉴 상태로 되돌린다.
	// bReloadWhenStandalone: 싱글 상태에서도 맵을 다시 열지. 인게임 이탈은 true, 이미 메뉴에 있으면 false.
	// 반환: 맵을 다시 열었거나 트래블이 걸렸으면 true.
	TR_API bool LeaveSessionAndReturnToMenu(const UObject* WorldContextObject, bool bReloadWhenStandalone);
}
