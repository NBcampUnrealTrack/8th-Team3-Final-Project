// TRMassDebugProcessor.cpp

#include "Mass/Debug/TRMassDebugProcessor.h"

#include "Animation/AnimSequenceTransformProviderData.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Engine/SkinnedAsset.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassNavigationFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationTypes.h"
#include "Mass/Debug/TRMassDebug.h"
#include "Mass/Core/TRMassFragments.h"
#include "Mass/Targeting/TRTargetDirectorSubsystem.h"

namespace
{
	/** 캡슐 크기. 사람 크기 기준이며, 바닥이 엔티티 원점에 오도록 중심을 올려 그린다. */
	constexpr float CapsuleHalfHeight = 88.f;
	constexpr float CapsuleRadius = 34.f;
	constexpr float ForwardArrowLength = 100.f;

	const TCHAR* LexRepresentation(EMassRepresentationType Type)
	{
		switch (Type)
		{
		case EMassRepresentationType::HighResSpawnedActor: return TEXT("HighResActor");
		case EMassRepresentationType::LowResSpawnedActor:  return TEXT("LowResActor");
		case EMassRepresentationType::StaticMeshInstance:  return TEXT("StaticMeshISM");
		// 이게 빠져 있어서 우리 적 전원이 "Other"로 찍혔다. 표현이 안 잡힌 것처럼 보여
		// 애니메이션을 엉뚱한 곳에서 찾게 만드는 표시라 반드시 이름이 있어야 한다.
		case EMassRepresentationType::SkinnedMeshInstance: return TEXT("SkinnedMeshISM");
		case EMassRepresentationType::None:                return TEXT("None");
		default:                                           return TEXT("Other");
		}
	}
}

UTRMassDebugProcessor::UTRMassDebugProcessor()
	: DrawQuery(*this)
	, StatusQuery(*this)
{
#if UE_BUILD_SHIPPING
	bAutoRegisterWithProcessingPhases = false;
#else
	bAutoRegisterWithProcessingPhases = true;
#endif

	ExecutionFlags = (int32)EProcessorExecutionFlags::AllNetModes;
	// 이동이 다 끝난 뒤의 최종 위치를 본다.
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::UpdateWorldFromMass;

	// 디버그 드로우와 서브시스템 조회는 게임 스레드에서만 안전하다.
	bRequiresGameThreadExecution = true;
}

void UTRMassDebugProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	DrawQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	DrawQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadOnly);
	DrawQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);

	StatusQuery.AddTagRequirement<FTRChaserTag>(EMassFragmentPresence::All);
	StatusQuery.AddRequirement<FMassRepresentationFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	StatusQuery.AddRequirement<FMassRepresentationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	StatusQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	StatusQuery.AddRequirement<FTRTargetFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
	StatusQuery.AddRequirement<FMassNavigationObstacleGridCellLocationFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

	// 이게 없으면 인스턴스 스킨드 메시가 T포즈로 고정된다. 엔진 프로세서가 필수로 요구하는데
	// 제공하는 트레이트가 없어서 빠뜨리기 쉬운 항목이라 진단에 포함한다.
	StatusQuery.AddRequirement<FMassRepresentationAnimationFragment>(
		EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
}

void UTRMassDebugProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
#if !UE_BUILD_SHIPPING
	// 1회 요청은 여기서 한 번만 소비한다. 여러 번 물어보면 첫 호출이 플래그를 가져간다.
	const bool bStatusRequested = UE::TR::MassDebug::ConsumeStatusRequest();
	const bool bStatusRepeat = UE::TR::MassDebug::IsStatusRepeatEnabled();
	const bool bDrawEnabled = UE::TR::MassDebug::IsDrawEnabled();
	const bool bDrawTarget = UE::TR::MassDebug::IsDrawTargetEnabled();

	// 전부 꺼져 있으면 즉시 빠져나간다. 평소에는 이 경로만 탄다.
	if (!bStatusRequested && !bStatusRepeat && !bDrawEnabled && !bDrawTarget)
	{
		return;
	}

	UWorld* World = EntityManager.GetWorld();
	if (World == nullptr)
	{
		return;
	}

	if (bDrawEnabled)
	{
		DrawEntities(Context, *World);
	}

	if (bDrawTarget)
	{
		DrawTargetBounds(Context, *World);
	}

	const bool bRepeatDue = bStatusRepeat && (World->GetTimeSeconds() >= NextLogTime);
	if (bStatusRequested || bRepeatDue)
	{
		NextLogTime = World->GetTimeSeconds() + 1.0;
		// 렌더 파이프라인 덤프는 컴포넌트마다 한 줄씩 나가므로 매초 찍으면 스팸이다.
		// 세션당 한 번은 자동으로 남기고(초기 구성 확인용), 그 뒤로는 1회 요청에만 응한다.
		const bool bIncludeRenderDetails = bStatusRequested || !bLoggedRenderDetailsOnce;
		bLoggedRenderDetailsOnce = true;

		LogStatus(Context, *World, bIncludeRenderDetails);
	}
#endif // !UE_BUILD_SHIPPING
}

#if !UE_BUILD_SHIPPING

void UTRMassDebugProcessor::DrawEntities(FMassExecutionContext& Context, UWorld& World)
{
	DrawQuery.ForEachEntityChunk(Context, [&World](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FTransformFragment> TransformList = ChunkContext.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FTRTargetFragment> TargetList = ChunkContext.GetFragmentView<FTRTargetFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransform& Transform = TransformList[EntityIt].GetTransform();
			const FTRTargetFragment& Target = TargetList[EntityIt];

			// 도달했으면 초록, 추적 중이면 노랑, 목표가 없으면 빨강.
			const FColor Color = !Target.bHasTarget ? FColor::Red
				: (Target.bArrived ? FColor::Green : FColor::Yellow);

			const FVector Location = Transform.GetLocation();
			const FVector CapsuleCenter = Location + FVector(0.f, 0.f, CapsuleHalfHeight);

			DrawDebugCapsule(&World, CapsuleCenter, CapsuleHalfHeight, CapsuleRadius,
				Transform.GetRotation(), Color, /*bPersistentLines=*/false, /*LifeTime=*/-1.f);

			DrawDebugDirectionalArrow(&World, CapsuleCenter,
				CapsuleCenter + Transform.GetRotation().GetForwardVector() * ForwardArrowLength,
				20.f, Color, false, -1.f);

			if (Target.bHasTarget && !Target.bArrived)
			{
				DrawDebugLine(&World, Location, Target.TargetLocation, FColor(80, 80, 80), false, -1.f, 0, 1.f);
			}
		}
	});
}

void UTRMassDebugProcessor::DrawTargetBounds(FMassExecutionContext& Context, UWorld& World)
{
	const UTRTargetDirectorSubsystem* Director = World.GetSubsystem<UTRTargetDirectorSubsystem>();
	if (Director == nullptr)
	{
		return;
	}

	// ── 채널별 충돌 영역 ──────────────────────────────────────────────────
	// 엔티티 쿼리를 거치지 않으므로 적이 하나도 없어도 그려진다.
	// "적이 안 오는 건지, 목표가 없는 건지"를 스폰 전에 확인할 수 있다.
	for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ETRTargetChannel::MAX); ++ChannelIndex)
	{
		FTRResolvedTarget Resolved;
		if (!Director->GetResolvedTarget(static_cast<ETRTargetChannel>(ChannelIndex), Resolved))
		{
			continue;
		}

		// 구역이 있으면 구역별로 그린다. 초록=살아 있음 / 빨강=파괴되어 표적에서 빠짐.
		if (Resolved.ZoneCount > 0)
		{
			for (int32 ZoneIndex = 0; ZoneIndex < Resolved.ZoneCount; ++ZoneIndex)
			{
				if (!Resolved.ZoneAlive[ZoneIndex])
				{
					continue;
				}

				// 등급을 색으로 나눈다. 청록=포탑(먼저 노림) / 초록=장갑.
				const bool bTurret = Resolved.ZoneTier[ZoneIndex] == ETRTargetZoneTier::Turret;
				const FColor ZoneColor = bTurret ? FColor::Cyan : FColor::Green;

				const FBox& Zone = Resolved.ZoneBounds[ZoneIndex];
				DrawDebugBox(&World, Zone.GetCenter(), Zone.GetExtent(), FQuat::Identity,
					ZoneColor, /*bPersistentLines=*/false, /*LifeTime=*/-1.f,
					/*DepthPriority=*/0, /*Thickness=*/3.f);

				DrawDebugString(&World, Zone.GetCenter(),
					FString::Printf(TEXT("%s%d"), bTurret ? TEXT("T") : TEXT("A"), ZoneIndex),
					nullptr, ZoneColor, 0.f, /*bDrawShadow=*/true);
			}

			continue;
		}

		if (!Resolved.bHasBounds)
		{
			// 바운드가 없으면 중심 한 점이 목표다. 이 상태면 적들이 한 점으로 수렴하므로
			// 눈에 띄게 표시한다(고정 위치 목표이거나 바운드 계산에 실패한 경우).
			DrawDebugSphere(&World, Resolved.Center, 100.f, 12, FColor::Magenta, false, -1.f, 0, 3.f);
			continue;
		}

		const FTransform& Transform = Resolved.Transform;

		// 로컬 박스라 액터 회전을 함께 넘겨야 기차가 돌 때 같이 돈다.
		// 스케일까지 반영하려면 Extent에 곱해야 한다(기차에 스케일이 들어간 경우).
		const FVector Center = Transform.TransformPosition(Resolved.LocalBounds.GetCenter());
		const FVector Extent = Resolved.LocalBounds.GetExtent() * Transform.GetScale3D();

		DrawDebugBox(&World, Center, Extent, Transform.GetRotation(),
			FColor::Cyan, /*bPersistentLines=*/false, /*LifeTime=*/-1.f, /*DepthPriority=*/0, /*Thickness=*/3.f);
	}

	// ── 각 적이 향하는 표면 최근접점 ──────────────────────────────────────
	// 점들이 박스 면에 고르게 붙어 있으면 표면 추적이 정상이다.
	// 한 점에 뭉쳐 있으면 바운드를 못 잡아 중심을 쫓고 있다는 뜻이다.
	DrawQuery.ForEachEntityChunk(Context, [&World](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FTRTargetFragment> TargetList = ChunkContext.GetFragmentView<FTRTargetFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTRTargetFragment& Target = TargetList[EntityIt];
			if (!Target.bHasTarget)
			{
				continue;
			}

			DrawDebugPoint(&World, Target.TargetLocation, 12.f,
				Target.bArrived ? FColor::Red : FColor::Orange, false, -1.f);
		}
	});
}

void UTRMassDebugProcessor::LogStatus(FMassExecutionContext& Context, UWorld& World, bool bIncludeRenderDetails)
{
	int32 TotalCount = 0;
	int32 NoRepresentationFragmentCount = 0;
	int32 NoActorFragmentCount = 0;
	int32 ValidActorCount = 0;
	int32 NoObstacleGridFragmentCount = 0;
	int32 NoAnimFragmentCount = 0;
	int32 NoTargetFragmentCount = 0;
	int32 HasTargetCount = 0;
	int32 ArrivedCount = 0;
	int32 LODCounts[EMassLOD::Max] = { 0 };
	TMap<EMassRepresentationType, int32> RepresentationCounts;

	/** 실제로 요청 중인 시퀀스 인덱스 분포. T포즈 진단의 핵심 값. */
	TMap<int32, int32> SequenceIndexCounts;

	StatusQuery.ForEachEntityChunk(Context, [&](FMassExecutionContext& ChunkContext)
	{
		const TConstArrayView<FMassRepresentationFragment> RepresentationList = ChunkContext.GetFragmentView<FMassRepresentationFragment>();
		const TConstArrayView<FMassRepresentationLODFragment> LODList = ChunkContext.GetFragmentView<FMassRepresentationLODFragment>();
		const TConstArrayView<FMassActorFragment> ActorList = ChunkContext.GetFragmentView<FMassActorFragment>();
		const TConstArrayView<FTRTargetFragment> TargetList = ChunkContext.GetFragmentView<FTRTargetFragment>();
		const TConstArrayView<FMassNavigationObstacleGridCellLocationFragment> ObstacleGridList =
			ChunkContext.GetFragmentView<FMassNavigationObstacleGridCellLocationFragment>();
		const TConstArrayView<FMassRepresentationAnimationFragment> AnimList =
			ChunkContext.GetFragmentView<FMassRepresentationAnimationFragment>();

		const bool bHasRepresentation = (RepresentationList.Num() > 0);
		const bool bHasLOD = (LODList.Num() > 0);
		const bool bHasActor = (ActorList.Num() > 0);
		const bool bHasTargetFragment = (TargetList.Num() > 0);
		const bool bHasObstacleGrid = (ObstacleGridList.Num() > 0);
		const bool bHasAnimFragment = (AnimList.Num() > 0);

		for (FMassExecutionContext::FEntityIterator EntityIt = ChunkContext.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			++TotalCount;

			if (bHasRepresentation)
			{
				RepresentationCounts.FindOrAdd(RepresentationList[EntityIt].CurrentRepresentation)++;
			}
			else
			{
				++NoRepresentationFragmentCount;
			}

			if (bHasLOD)
			{
				const int32 LODIndex = static_cast<int32>(LODList[EntityIt].LOD);
				if (LODIndex >= 0 && LODIndex < EMassLOD::Max)
				{
					++LODCounts[LODIndex];
				}
			}

			if (!bHasActor)
			{
				++NoActorFragmentCount;
			}
			else if (ActorList[EntityIt].Get() != nullptr)
			{
				++ValidActorCount;
			}

			if (!bHasObstacleGrid)
			{
				++NoObstacleGridFragmentCount;
			}

			if (!bHasAnimFragment)
			{
				++NoAnimFragmentCount;
			}
			else
			{
				// 어떤 시퀀스가 실제로 요청되고 있는지. T포즈 원인을 가르는 값이다 —
				// 인덱스가 골고루 나오면 선택 프로세서는 정상이고 하류(프로바이더/GPU)가
				// 문제, 전부 0이면 선택 프로세서가 아예 안 돌고 있다는 뜻이다.
				const FAnimSequenceTrackAutoPlayData& AnimData = AnimList[EntityIt].AnimData;
				SequenceIndexCounts.FindOrAdd(AnimData.SequenceIndex)++;
			}

			if (!bHasTargetFragment)
			{
				++NoTargetFragmentCount;
			}
			else
			{
				if (TargetList[EntityIt].bHasTarget)
				{
					++HasTargetCount;
				}
				if (TargetList[EntityIt].bArrived)
				{
					++ArrivedCount;
				}
			}
		}
	});

	UE_LOG(LogTRMass, Warning, TEXT("엔티티 %d | 표현프래그먼트없음 %d | 액터프래그먼트없음 %d | 살아있는액터 %d"),
		TotalCount, NoRepresentationFragmentCount, NoActorFragmentCount, ValidActorCount);

	UE_LOG(LogTRMass, Warning, TEXT("LOD  High=%d Medium=%d Low=%d Off=%d"),
		LODCounts[EMassLOD::High], LODCounts[EMassLOD::Medium], LODCounts[EMassLOD::Low], LODCounts[EMassLOD::Off]);

	for (const TPair<EMassRepresentationType, int32>& Pair : RepresentationCounts)
	{
		UE_LOG(LogTRMass, Warning, TEXT("표현  %s = %d"), LexRepresentation(Pair.Key), Pair.Value);
	}

	UE_LOG(LogTRMass, Warning, TEXT("목표  보유 %d / 도달 %d / 타깃프래그먼트없음 %d"),
		HasTargetCount, ArrivedCount, NoTargetFragmentCount);

	if (NoAnimFragmentCount > 0)
	{
		UE_LOG(LogTRMass, Error, TEXT("애님  프래그먼트 없음 %d — 이 개체들은 T포즈로 고정된다"),
			NoAnimFragmentCount);
	}

	for (const TPair<int32, int32>& Pair : SequenceIndexCounts)
	{
		UE_LOG(LogTRMass, Warning, TEXT("애님  SequenceIndex[%d] = %d"), Pair.Key, Pair.Value);
	}

	if (!bIncludeRenderDetails)
	{
		return;
	}

	// 인덱스는 잘 쓰이는데 T포즈가 나오면 남은 곳은 여기뿐이다 — 컴포넌트에 프로바이더가
	// 실제로 붙었는가, 그 안에 시퀀스가 몇 개인가. 붙지 않아도 경고가 안 나므로 직접 본다.
	for (TObjectIterator<UInstancedSkinnedMeshComponent> It; It; ++It)
	{
		UInstancedSkinnedMeshComponent* Component = *It;
		if (Component == nullptr || Component->GetWorld() != &World)
		{
			continue;
		}

		const UTransformProviderData* Provider = Component->GetTransformProvider();
		const UAnimSequenceTransformProviderDataInstance* Instance =
			Cast<UAnimSequenceTransformProviderDataInstance>(Provider);

		// **T포즈 판정의 핵심.** 프로바이더는 IsValidFor 에서 이 조건을 요구하고,
		// 실패하면 프록시를 만들지 않아 레퍼런스 포즈(T포즈)로 그려진다.
		// 이게 false 면 스켈레탈 메시 Build Settings 의 'Optimize for Instancing' 이 꺼진 것이다.
		const USkinnedAsset* SkinnedAsset = Component->GetSkinnedAsset();
		const FSkeletalMeshRenderData* RenderData =
			SkinnedAsset ? SkinnedAsset->GetResourceForRendering() : nullptr;
		const bool bUnifiedBoneMap = RenderData && RenderData->HasUnifiedBoneMap();

		// AnimationMinScreenSize 는 protected 라 게터가 없다. 디버그 목적이라 리플렉션으로 읽는다.
		// 0 이 아니면 전역 r.Skinning.DefaultAnimationMinScreenSize 를 무시하므로,
		// 콘솔로 전역값을 낮춰도 T포즈가 그대로인 상황의 원인이 될 수 있다.
		// **빈 칸이 하나라도 있으면 프로바이더 전체가 무효가 된다**(IsValidFor 가 false 를 반환).
		// 그러면 인덱스가 아무리 정확해도 전원 레퍼런스 포즈(T포즈)로 그려진다.
		// 개수만 세면 null 칸도 함께 세어져 정상으로 보이므로 따로 표시한다.
		int32 NullSequenceCount = 0;
		if (Instance != nullptr)
		{
			for (const FAnimSequenceTransformProviderSequence& Sequence : Instance->GetSequences())
			{
				if (Sequence.Sequence == nullptr)
				{
					++NullSequenceCount;
				}
			}
		}

		float AnimMinScreenSize = -999.f;
		if (const FFloatProperty* Prop = CastField<FFloatProperty>(
			UInstancedSkinnedMeshComponent::StaticClass()->FindPropertyByName(TEXT("AnimationMinScreenSize"))))
		{
			AnimMinScreenSize = Prop->GetPropertyValue_InContainer(Component);
		}

		UE_LOG(LogTRMass, Warning,
			TEXT("ISM  '%s' 메시=%s | 인스턴스=%d | 프로바이더=%s | 시퀀스=%d | UnifiedBoneMap=%s | AnimMinScreenSize=%.4f"),
			*Component->GetName(),
			*GetNameSafe(SkinnedAsset),
			Component->GetInstanceCount(),
			Provider ? *Provider->GetClass()->GetName() : TEXT("(없음!)"),
			Instance ? Instance->GetSequences().Num() : -1,
			bUnifiedBoneMap ? TEXT("O") : TEXT("X ← Optimize for Instancing 꺼짐"),
			AnimMinScreenSize);

		if (NullSequenceCount > 0)
		{
			UE_LOG(LogTRMass, Error,
				TEXT("ISM  ↑ 시퀀스 목록에 빈 칸 %d개 — **프로바이더 전체가 무효가 되어 전원 T포즈다.** "
					 "Sequence List 의 빈 칸을 채우고, TPD 를 다시 저장할 것."),
				NullSequenceCount);
		}
	}

	// 엔티티가 목표를 못 받는 경우, 원인이 Director에 있는지 전파에 있는지 갈라야 한다.
	if (const UTRTargetDirectorSubsystem* Director = World.GetSubsystem<UTRTargetDirectorSubsystem>())
	{
		for (int32 ChannelIndex = 0; ChannelIndex < static_cast<int32>(ETRTargetChannel::MAX); ++ChannelIndex)
		{
			FTRResolvedTarget Resolved;
			const bool bHasTarget = Director->GetResolvedTarget(
				static_cast<ETRTargetChannel>(ChannelIndex), Resolved);

			// 영역 유무까지 함께 찍는다. 목표는 있는데 영역이 없으면 적들이 중심
			// 한 점으로 수렴하므로, 그 상태를 로그만 보고도 구분할 수 있어야 한다.
			UE_LOG(LogTRMass, Warning, TEXT("Director 채널[%d] %s %s %s"),
				ChannelIndex,
				bHasTarget ? TEXT("설정됨") : TEXT("── 목표 없음 ──"),
				bHasTarget ? *Resolved.Center.ToCompactString() : TEXT(""),
				bHasTarget ? (Resolved.bHasBounds ? TEXT("(영역 있음)") : TEXT("(영역 없음 — 중심 추적)")) : TEXT(""));
		}
	}
	else
	{
		UE_LOG(LogTRMass, Error, TEXT("TargetDirectorSubsystem을 찾을 수 없습니다."));
	}

	if (NoAnimFragmentCount > 0)
	{
		UE_LOG(LogTRMass, Error,
			TEXT("인스턴스 애니메이션 비활성: %d개 엔티티에 RepresentationAnimation 프래그먼트가 없습니다. ")
			TEXT("인스턴스 표현이 T포즈로 고정됩니다."),
			NoAnimFragmentCount);
	}

	if (NoObstacleGridFragmentCount > 0)
	{
		UE_LOG(LogTRMass, Error,
			TEXT("분리력 비활성: %d개 엔티티에 해시 그리드 프래그먼트가 없습니다. ")
			TEXT("Config에 'Navigation Obstacle' 트레이트를 추가하세요."),
			NoObstacleGridFragmentCount);
	}
	else
	{
		UE_LOG(LogTRMass, Warning, TEXT("분리력  그리드 등록 정상 (%d)"), TotalCount);
	}
}

#endif // !UE_BUILD_SHIPPING
