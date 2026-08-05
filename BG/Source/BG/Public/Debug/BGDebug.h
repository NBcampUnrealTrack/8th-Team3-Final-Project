// BANG! 화면 디버그 — 뷰포트 좌상단에 색 메시지를 띄운다.
//   콘솔변수 Bang.Debug 로 켜고 끈다 (기본 1). 0 이면 조용해진다.
//   ⚠️ GA 는 ServerOnly 라 이 메시지는 "서버(리슨서버 호스트) 화면"에 뜬다.
//      클라 창에서도 보이게 하려면 Multicast/Client RPC 로 감싸야 한다.

#pragma once

#include "CoreMinimal.h"

namespace BGDebug
{
	// Bang.Debug != 0 이면 true.
	BG_API bool IsEnabled();

	// 화면에 색 메시지. Key<0 이면 계속 쌓이고, Key>=0 이면 같은 슬롯을 갱신한다.
	BG_API void Screen(const FString& Msg, const FColor& Color = FColor::White,
		float Time = 4.f, int32 Key = INDEX_NONE);
}
