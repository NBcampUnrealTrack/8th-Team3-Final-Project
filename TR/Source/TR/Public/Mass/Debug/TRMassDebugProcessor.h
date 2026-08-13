// TRMassDebugProcessor.h

#pragma once

#include "CoreMinimal.h"
#include "MassEntityQuery.h"
#include "MassProcessor.h"
#include "TRMassDebugProcessor.generated.h"

class UWorld;

/**
 * TR Mass의 런타임 진단 하나로 모은 프로세서. 그리기와 상태 로그를 함께 담당한다.
 *
 * 이 프로젝트에서 Mass는 대부분 **조용히 실패**한다. 트레이트가 빠지거나 프로세서가
 * 등록되지 않아도 에러가 없고, 설정 화면만 봐서는 알 수 없다. 그래서 "설정이 맞는가"가
 * 아니라 "런타임에 실제로 어떤 상태인가"를 보는 이 경로가 진단의 출발점이 된다.
 *
 * 명령 목록은 TRMassDebug.cpp 상단 주석에 정리되어 있다.
 */
UCLASS()
class TR_API UTRMassDebugProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UTRMassDebugProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:

	void DrawEntities(FMassExecutionContext& Context, UWorld& World);

	/**
	 * 추적 목표의 충돌 영역(바운드 박스)과 각 적이 향하는 표면 최근접점.
	 *
	 * 접촉 판정이 물리가 아니라 이 박스까지의 거리로 이뤄지므로, 박스가 실제 기차
	 * 실루엣과 얼마나 어긋나는지가 곧 자폭 타이밍의 오차다. 눈으로 봐야 조절할 수 있다.
	 */
	void DrawTargetBounds(FMassExecutionContext& Context, UWorld& World);

	/** 엔티티 수, 프래그먼트 유무, LOD/표현 분포, 목표 상태를 로그로 덤프한다. */
	void LogStatus(FMassExecutionContext& Context, UWorld& World);

	/** 그리기용. 필요한 것만 요구한다. */
	FMassEntityQuery DrawQuery;

	/**
	 * 진단용. 관련 프래그먼트를 모두 Optional로 잡는다.
	 * "값이 이상하다"와 "프래그먼트가 아예 없다"를 구분해야 원인이 갈린다.
	 */
	FMassEntityQuery StatusQuery;

	double NextLogTime = 0.0;
};
