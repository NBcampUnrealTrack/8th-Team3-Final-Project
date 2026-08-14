// TRGameplayLog.h

// TR 게임플레이 전용 로그 카테고리 — 상호작용/수리/탑승/사격 흐름만 골라 보기 위함.
// Session/TRNetLog.h 의 LogTRNet 과 같은 패턴.
//
// 콘솔에서 Verbose 레벨 Log 보려면:  Log LogTRGameplay Verbose

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTRGameplay, Log, All);
