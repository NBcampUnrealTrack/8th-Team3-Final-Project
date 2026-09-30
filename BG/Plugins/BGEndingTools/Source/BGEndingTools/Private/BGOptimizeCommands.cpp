// 맵 최적화 에디터 명령 — 에디터 콘솔(Output Log 의 Cmd)에서 친다.
//
//   BG.Optimize.CullDistance          크기 분포와 적용될 거리만 출력한다(맵을 바꾸지 않음).
//   BG.Optimize.CullDistance apply    Cull Distance Volume 을 만들거나 갱신한다.
//   BG.Optimize.SmallShadows          그림자를 끌 작은 소품 수만 출력한다.
//   BG.Optimize.SmallShadows apply    작은 소품의 Cast Shadow 를 끈다.
//   BG.Optimize.Instance              인스턴스로 묶일 메시와 줄어들 액터 수만 출력한다.
//   BG.Optimize.Instance apply        반복 메시를 셀별 HISM 액터로 묶고 원본을 지운다.
//   BG.Optimize.HLODReport            스트리밍·HLOD 설정과 카메라→맵 끝 거리를 출력한다(읽기 전용).
//
// 전부 트랜잭션이라 Ctrl+Z 로 되돌린다. 맵 저장은 직접 한다.
//
// ⚠️ Cull Distance Volume 은 엔진이 Static 메시에만 적용한다(ACullDistanceVolume::CanBeAffectedByVolumes).
//    바람 피벗에 붙은 Movable 나무·숨겨 둔 엔딩 GC 는 자동으로 빠진다.

#include "CoreMinimal.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshComponentLODInfo.h"
#include "Editor.h"
#include "Engine/CullDistanceVolume.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ScopedTransaction.h"
#include "UObject/UObjectHash.h"
#include "WorldPartition/WorldPartition.h"

DEFINE_LOG_CATEGORY_STATIC(LogBGOptimize, Log, All);

namespace BGOptimize
{
	//	같은 볼륨을 다시 찾는 표식. 라벨은 사람이 바꿀 수 있어 태그로 찾는다.
	static const FName CullVolumeTag(TEXT("BG_OptCullDistance"));

	//	지름(cm) → 그리기 거리(cm). 엔진은 지름이 **가장 가까운** 줄을 고른다. 0 = 끝까지 그린다.
	//	⚠️ 카메라가 캠프파이어에 고정된 판 화면과, 성을 멀리서 잡는 엔딩 샷 둘 다 보고 정한 값이다.
	//	   큰 건물(성벽·탑)은 엔딩 원경에 나오므로 자르지 않는다.
	static const FCullDistanceSizePair CullTable[] =
	{
		{   50.f,  3000.f },   // 풀 한 포기·작은 돌
		{  150.f,  6000.f },   // 소품·랜턴·덤불
		{  400.f, 12000.f },   // 바위·작은 나무
		{ 1000.f, 25000.f },   // 큰 나무·작은 구조물
		{ 3000.f,     0.f },   // 건물·성벽 — 자르지 않는다
	};

	//	이보다 작은 소품은 그림자를 끈다. 판 화면 거리에서는 그림자가 거의 안 보인다.
	static constexpr float SmallShadowDiameter = 120.f;

	static UWorld* GetEditorWorld()
	{
		if (!GEditor)
		{
			return nullptr;
		}
		//	PIE 중에는 편집 월드를 건드리지 않는다 — 결과가 PIE 월드와 섞여 보인다.
		if (GEditor->PlayWorld)
		{
			UE_LOG(LogBGOptimize, Warning, TEXT("PIE 를 끄고 실행할 것."));
			return nullptr;
		}
		return GEditor->GetEditorWorldContext().World();
	}

	static float GetCullDistanceFor(float Diameter)
	{
		float BestError = FLT_MAX;
		float Result = 0.f;
		for (const FCullDistanceSizePair& Pair : CullTable)
		{
			const float Error = FMath::Abs(Diameter - Pair.Size);
			if (Error < BestError)
			{
				BestError = Error;
				Result = Pair.CullDistance;
			}
		}
		return Result;
	}

	static bool IsApply(const TArray<FString>& Args)
	{
		return Args.Num() > 0 && Args[0].Equals(TEXT("apply"), ESearchCase::IgnoreCase);
	}

	static void RunCullDistance(const TArray<FString>& Args)
	{
		UWorld* World = GetEditorWorld();
		if (!World)
		{
			return;
		}

		//	줄마다 몇 개가 걸리는지 먼저 보여 준다. 값을 고칠 근거가 이것뿐이다.
		TMap<float, int32> CountByDistance;
		int32 NumStatic = 0;
		int32 NumSkippedMovable = 0;
		FBox Bounds(ForceInit);

		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			const UStaticMeshComponent* Comp = It->GetStaticMeshComponent();
			if (!Comp || !Comp->GetStaticMesh())
			{
				continue;
			}

			Bounds += Comp->Bounds.GetBox();

			if (Comp->Mobility != EComponentMobility::Static)
			{
				++NumSkippedMovable;
				continue;
			}

			++NumStatic;
			CountByDistance.FindOrAdd(GetCullDistanceFor(Comp->Bounds.SphereRadius * 2.f))++;
		}

		if (!Bounds.IsValid)
		{
			UE_LOG(LogBGOptimize, Warning, TEXT("StaticMeshActor 가 없다."));
			return;
		}

		UE_LOG(LogBGOptimize, Log, TEXT("[CullDistance] Static %d 개 (Movable %d 개는 볼륨 대상 아님). 맵 크기 %.0f x %.0f x %.0f cm"),
			NumStatic, NumSkippedMovable, Bounds.GetSize().X, Bounds.GetSize().Y, Bounds.GetSize().Z);

		CountByDistance.KeySort(TLess<float>());
		for (const TPair<float, int32>& Pair : CountByDistance)
		{
			UE_LOG(LogBGOptimize, Log, TEXT("  그리기 거리 %s : %d 개"),
				Pair.Key > 0.f ? *FString::Printf(TEXT("%.0f m"), Pair.Key / 100.f) : TEXT("제한 없음"), Pair.Value);
		}

		if (!IsApply(Args))
		{
			UE_LOG(LogBGOptimize, Log, TEXT("[CullDistance] 미리보기만 했다. 적용하려면 'BG.Optimize.CullDistance apply'."));
			return;
		}

		const FScopedTransaction Transaction(NSLOCTEXT("BGOptimize", "CullDistance", "BG: Cull Distance Volume"));

		ACullDistanceVolume* Volume = nullptr;
		for (TActorIterator<ACullDistanceVolume> It(World); It; ++It)
		{
			if (It->Tags.Contains(CullVolumeTag))
			{
				Volume = *It;
				break;
			}
		}

		//	맵 전체를 덮는다. 경계에 걸친 액터는 중심이 안에 있어야 적용되므로 여유를 둔다.
		const FBox Covered = Bounds.ExpandBy(1000.f);

		if (!Volume)
		{
			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transactional;
			Volume = World->SpawnActor<ACullDistanceVolume>(Covered.GetCenter(), FRotator::ZeroRotator, Params);
			if (!Volume)
			{
				UE_LOG(LogBGOptimize, Error, TEXT("[CullDistance] 볼륨을 만들지 못했다."));
				return;
			}
			Volume->Tags.Add(CullVolumeTag);
			Volume->SetActorLabel(TEXT("OPT_CullDistance"));
		}

		Volume->Modify();

		//	⚠️ 월드 파티션에서 공간 로딩되면 셀이 안 불렸을 때 볼륨이 없어 컬링이 풀린다.
		Volume->SetIsSpatiallyLoaded(false);

		UCubeBuilder* Builder = NewObject<UCubeBuilder>();
		const FVector Size = Covered.GetSize();
		Builder->X = Size.X;
		Builder->Y = Size.Y;
		Builder->Z = Size.Z;
		UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
		Volume->SetActorLocation(Covered.GetCenter());

		Volume->CullDistances.Reset();
		for (const FCullDistanceSizePair& Pair : CullTable)
		{
			Volume->CullDistances.Add(Pair);
		}
		Volume->bEnabled = true;

		Volume->PostEditChange();
		World->UpdateCullDistanceVolumes();
		Volume->MarkPackageDirty();

		UE_LOG(LogBGOptimize, Log, TEXT("[CullDistance] 볼륨 적용 완료 — 맵을 저장할 것. 되돌리려면 Ctrl+Z."));
	}

	static void RunSmallShadows(const TArray<FString>& Args)
	{
		UWorld* World = GetEditorWorld();
		if (!World)
		{
			return;
		}

		const bool bApply = IsApply(Args);
		TOptional<FScopedTransaction> Transaction;
		if (bApply)
		{
			Transaction.Emplace(NSLOCTEXT("BGOptimize", "SmallShadows", "BG: 작은 소품 그림자 끄기"));
		}

		int32 NumTargets = 0;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			//	엔딩 시퀀스가 붙잡는 액터는 건드리지 않는다.
			if (It->GetActorLabel().StartsWith(TEXT("ENDING_")))
			{
				continue;
			}

			UStaticMeshComponent* Comp = It->GetStaticMeshComponent();
			if (!Comp || !Comp->GetStaticMesh() || !Comp->CastShadow)
			{
				continue;
			}
			if (Comp->Bounds.SphereRadius * 2.f > SmallShadowDiameter)
			{
				continue;
			}

			++NumTargets;
			if (bApply)
			{
				Comp->Modify();
				Comp->SetCastShadow(false);
				It->MarkPackageDirty();
			}
		}

		UE_LOG(LogBGOptimize, Log, TEXT("[SmallShadows] 지름 %.0fcm 이하 그림자 켜진 소품 %d 개%s"),
			SmallShadowDiameter, NumTargets,
			bApply ? TEXT(" — 그림자를 껐다. 맵을 저장할 것.") : TEXT(". 적용하려면 'BG.Optimize.SmallShadows apply'."));
	}

	// ── 인스턴싱 ──────────────────────────────────────────────────────────

	//	같은 메시가 이 수 이상 있어야 묶는다. 한두 개짜리는 묶어도 이득이 없고 편집만 불편해진다.
	static constexpr int32 MinInstancesPerMesh = 8;

	//	셀 크기(cm). 셀마다 액터 하나 — 한 액터로 맵 전체를 덮으면 월드 파티션 스트리밍과
	//	컴포넌트 단위 컬링이 전부 무력해진다.
	static constexpr double InstanceCellSize = 5000.0;

	static const FName InstanceFolder(TEXT("OPT_Instances"));

	//	같은 HISM 에 넣어도 되는가를 가르는 값. 하나라도 다르면 원본과 다르게 보이거나 충돌한다.
	struct FInstanceKey
	{
		const UStaticMesh* Mesh = nullptr;
		TArray<const UMaterialInterface*> Materials;
		FName CollisionProfile;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
		bool bCastShadow = true;
		FIntVector Cell;

		bool operator==(const FInstanceKey& Other) const
		{
			return Mesh == Other.Mesh && Materials == Other.Materials
				&& CollisionProfile == Other.CollisionProfile && CollisionEnabled == Other.CollisionEnabled
				&& bCastShadow == Other.bCastShadow && Cell == Other.Cell;
		}

		friend uint32 GetTypeHash(const FInstanceKey& Key)
		{
			uint32 Hash = HashCombine(GetTypeHash(Key.Mesh), GetTypeHash(Key.Cell));
			Hash = HashCombine(Hash, GetTypeHash(Key.CollisionProfile));
			return HashCombine(Hash, GetTypeHash(Key.bCastShadow));
		}
	};

	//	묶어도 안전한 액터인가. 조금이라도 의심스러우면 뺀다 — 빠진 건 성능만 손해지만,
	//	잘못 묶인 건 연출이나 게임 로직이 조용히 깨진다.
	static UStaticMeshComponent* GetInstanceCandidate(AStaticMeshActor* Actor)
	{
		UStaticMeshComponent* Comp = Actor ? Actor->GetStaticMeshComponent() : nullptr;
		if (!Comp || !Comp->GetStaticMesh() || Comp->Mobility != EComponentMobility::Static)
		{
			return nullptr;   // Movable 은 바람 피벗에 붙은 것들이다
		}

		const FString Label = Actor->GetActorLabel();
		if (Label.StartsWith(TEXT("ENDING_")) || Label.StartsWith(TEXT("CAMPLAY_")) || Label.StartsWith(TEXT("OPT_")))
		{
			return nullptr;   // 시퀀스·바람 연출이 이름으로 묶어 둔 것
		}

		//	붙어 있거나 붙은 게 있으면 원본을 지울 때 계층이 깨진다.
		TArray<AActor*> Attached;
		Actor->GetAttachedActors(Attached);
		if (Actor->GetAttachParentActor() || Attached.Num() > 0)
		{
			return nullptr;
		}

		//	태그·데이터 레이어는 누군가 이 액터를 따로 찾거나 켜고 끈다는 뜻이다.
		if (Actor->Tags.Num() > 0 || Actor->HasDataLayers())
		{
			return nullptr;
		}

		//	숨겨 둔 것은 누군가 켜려고 둔 것이다. HISM 으로 옮기면 그냥 보인다.
		if (Actor->IsHidden() || !Comp->IsVisible())
		{
			return nullptr;
		}

		//	인스턴스는 버텍스 컬러 페인트를 담지 못한다 — 옮기면 칠한 색이 사라진다.
		for (const FStaticMeshComponentLODInfo& LOD : Comp->LODData)
		{
			if (LOD.OverrideVertexColors)
			{
				return nullptr;
			}
		}

		//	메시 말고 다른 컴포넌트(라이트·파티클 등)가 붙어 있으면 인스턴스로 옮길 수 없다.
		TInlineComponentArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		if (Components.Num() != 1)
		{
			return nullptr;
		}

		return Comp;
	}

	static void RunInstance(const TArray<FString>& Args)
	{
		UWorld* World = GetEditorWorld();
		if (!World)
		{
			return;
		}

		//	메시별 총 개수를 먼저 센다 — 셀로 나누기 전 기준으로 MinInstancesPerMesh 를 본다.
		TMap<const UStaticMesh*, int32> CountByMesh;
		TArray<TPair<AStaticMeshActor*, UStaticMeshComponent*>> Candidates;
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			if (UStaticMeshComponent* Comp = GetInstanceCandidate(*It))
			{
				Candidates.Emplace(*It, Comp);
				CountByMesh.FindOrAdd(Comp->GetStaticMesh())++;
			}
		}

		TMap<FInstanceKey, TArray<TPair<AStaticMeshActor*, UStaticMeshComponent*>>> Groups;
		for (const TPair<AStaticMeshActor*, UStaticMeshComponent*>& Candidate : Candidates)
		{
			UStaticMeshComponent* Comp = Candidate.Value;
			if (CountByMesh[Comp->GetStaticMesh()] < MinInstancesPerMesh)
			{
				continue;
			}

			FInstanceKey Key;
			Key.Mesh = Comp->GetStaticMesh();
			for (int32 Index = 0; Index < Comp->GetNumMaterials(); ++Index)
			{
				Key.Materials.Add(Comp->GetMaterial(Index));
			}
			Key.CollisionProfile = Comp->GetCollisionProfileName();
			Key.CollisionEnabled = Comp->GetCollisionEnabled();
			Key.bCastShadow = Comp->CastShadow;

			const FVector Location = Comp->GetComponentLocation();
			Key.Cell = FIntVector(
				FMath::FloorToInt(Location.X / InstanceCellSize),
				FMath::FloorToInt(Location.Y / InstanceCellSize),
				0);

			Groups.FindOrAdd(Key).Add(Candidate);
		}

		//	혼자 남은 셀 그룹은 묶어 봐야 액터 수가 그대로다.
		int32 NumRemoved = 0;
		int32 NumCreated = 0;
		TMap<const UStaticMesh*, int32> InstancedByMesh;
		for (const auto& Pair : Groups)
		{
			if (Pair.Value.Num() < 2)
			{
				continue;
			}
			NumRemoved += Pair.Value.Num();
			++NumCreated;
			InstancedByMesh.FindOrAdd(Pair.Key.Mesh) += Pair.Value.Num();
		}

		UE_LOG(LogBGOptimize, Log, TEXT("[Instance] 후보 %d 개 중 %d 개를 %d 개 HISM 액터로 묶는다 (액터 %d 개 감소)."),
			Candidates.Num(), NumRemoved, NumCreated, NumRemoved - NumCreated);

		InstancedByMesh.ValueSort(TGreater<int32>());
		for (const TPair<const UStaticMesh*, int32>& Pair : InstancedByMesh)
		{
			UE_LOG(LogBGOptimize, Log, TEXT("  %s : %d 개"), *GetNameSafe(Pair.Key), Pair.Value);
		}

		if (!IsApply(Args))
		{
			UE_LOG(LogBGOptimize, Log, TEXT("[Instance] 미리보기만 했다. 적용 전에 커밋해 둘 것 — 'BG.Optimize.Instance apply'."));
			return;
		}

		const FScopedTransaction Transaction(NSLOCTEXT("BGOptimize", "Instance", "BG: 반복 메시 인스턴싱"));

		for (const auto& Pair : Groups)
		{
			const FInstanceKey& Key = Pair.Key;
			const TArray<TPair<AStaticMeshActor*, UStaticMeshComponent*>>& Members = Pair.Value;
			if (Members.Num() < 2)
			{
				continue;
			}

			//	액터 원점은 셀 안 첫 인스턴스 자리 — 셀 중심에 두면 에디터에서 빈 곳을 잡게 된다.
			const FTransform Origin(Members[0].Value->GetComponentLocation());

			FActorSpawnParameters Params;
			Params.ObjectFlags |= RF_Transactional;
			AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), Origin, Params);
			if (!Holder)
			{
				continue;
			}

			UHierarchicalInstancedStaticMeshComponent* HISM =
				NewObject<UHierarchicalInstancedStaticMeshComponent>(Holder, TEXT("Instances"), RF_Transactional);
			HISM->SetMobility(EComponentMobility::Static);
			HISM->SetStaticMesh(const_cast<UStaticMesh*>(Key.Mesh));
			for (int32 Index = 0; Index < Key.Materials.Num(); ++Index)
			{
				HISM->SetMaterial(Index, const_cast<UMaterialInterface*>(Key.Materials[Index]));
			}
			HISM->SetCollisionProfileName(Key.CollisionProfile);
			HISM->SetCollisionEnabled(Key.CollisionEnabled);
			HISM->SetCastShadow(Key.bCastShadow);

			//	⚠️ Cull Distance Volume 은 컴포넌트(=셀 전체) 크기로 거리를 고른다 — 인스턴스 하나의
			//	   크기와 달라 엉뚱한 거리가 나온다. 인스턴스 거리는 여기서 같은 표로 직접 준다.
			HISM->bAllowCullDistanceVolume = false;
			const float EndCull = GetCullDistanceFor(Members[0].Value->Bounds.SphereRadius * 2.f);
			if (EndCull > 0.f)
			{
				HISM->SetCullDistances(FMath::RoundToInt(EndCull * 0.8f), FMath::RoundToInt(EndCull));
			}

			Holder->SetRootComponent(HISM);
			Holder->AddInstanceComponent(HISM);
			HISM->RegisterComponent();
			Holder->SetActorTransform(Origin);

			TArray<FTransform> Transforms;
			Transforms.Reserve(Members.Num());
			for (const TPair<AStaticMeshActor*, UStaticMeshComponent*>& Member : Members)
			{
				Transforms.Add(Member.Value->GetComponentTransform());
			}
			HISM->AddInstances(Transforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/true);

			Holder->SetActorLabel(FString::Printf(TEXT("OPT_%s_%d_%d"), *GetNameSafe(Key.Mesh), Key.Cell.X, Key.Cell.Y));
			Holder->SetFolderPath(InstanceFolder);
			Holder->MarkPackageDirty();

			for (const TPair<AStaticMeshActor*, UStaticMeshComponent*>& Member : Members)
			{
				World->EditorDestroyActor(Member.Key, /*bShouldModifyLevel=*/true);
			}
		}

		UE_LOG(LogBGOptimize, Log, TEXT("[Instance] 적용 완료 — 화면 확인 후 맵을 저장할 것. 되돌리려면 Ctrl+Z."));
	}

	// ── HLOD 진단 ─────────────────────────────────────────────────────────

	//	기본값과 다른 프로퍼티만 찍는다. 스트리밍 설정은 private 라 리플렉션으로 읽는다.
	static void DumpNonDefaultProperties(const UObject* Object, const FString& Indent)
	{
		const UObject* Defaults = Object->GetClass()->GetDefaultObject();
		for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
		{
			const FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated))
			{
				continue;
			}

			const void* Value = Property->ContainerPtrToValuePtr<void>(Object);
			const void* DefaultValue = Property->ContainerPtrToValuePtr<void>(Defaults);
			if (Property->Identical(Value, DefaultValue))
			{
				continue;
			}

			FString Text;
			Property->ExportTextItem_Direct(Text, Value, nullptr, nullptr, PPF_None);
			UE_LOG(LogBGOptimize, Log, TEXT("%s%s = %s"), *Indent, *Property->GetName(), *Text.Left(300));
		}
	}

	static void RunHLODReport(const TArray<FString>& /*Args*/)
	{
		UWorld* World = GetEditorWorld();
		if (!World)
		{
			return;
		}

		UWorldPartition* WorldPartition = World->GetWorldPartition();
		if (!WorldPartition)
		{
			UE_LOG(LogBGOptimize, Log, TEXT("[HLOD] 월드 파티션 맵이 아니다 — HLOD 대상이 아니다."));
			return;
		}

		UE_LOG(LogBGOptimize, Log, TEXT("[HLOD] 스트리밍 %s"),
			WorldPartition->IsStreamingEnabled() ? TEXT("켜짐") : TEXT("꺼짐 — 전부 항상 로드된다. HLOD 는 쓰이지 않는다."));

		//	RuntimeHash 와 그 아래 파티션(그리드·HLOD 설정)의 값. LoadingRange·CellSize 가 판단 근거다.
		if (const FObjectProperty* HashProperty = FindFProperty<FObjectProperty>(UWorldPartition::StaticClass(), TEXT("RuntimeHash")))
		{
			if (const UObject* Hash = HashProperty->GetObjectPropertyValue_InContainer(WorldPartition))
			{
				UE_LOG(LogBGOptimize, Log, TEXT("[HLOD] %s"), *Hash->GetClass()->GetName());
				DumpNonDefaultProperties(Hash, TEXT("  "));

				TArray<UObject*> Partitions;
				GetObjectsWithOuter(Hash, Partitions, EGetObjectsFlags::IncludeNestedObjects);
				for (const UObject* Partition : Partitions)
				{
					UE_LOG(LogBGOptimize, Log, TEXT("  [%s] %s"), *Partition->GetClass()->GetName(), *Partition->GetName());
					DumpNonDefaultProperties(Partition, TEXT("    "));
				}
			}
		}

		//	카메라에서 가장 먼 메시까지의 거리. LoadingRange 보다 짧으면 전부 로드된 채라 HLOD 가 뜰 일이 없다.
		FBox MeshBounds(ForceInit);
		for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
		{
			MeshBounds += It->GetComponentsBoundingBox();
		}

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			const FString ClassName = It->GetClass()->GetName();
			const bool bCamera = ClassName.Contains(TEXT("Camera")) && !ClassName.Contains(TEXT("Manager"));
			const bool bCampfire = ClassName.Contains(TEXT("Campfire"));
			if (!bCamera && !bCampfire)
			{
				continue;
			}

			const FVector Location = It->GetActorLocation();
			double Farthest = 0.0;
			for (int32 Corner = 0; Corner < 8; ++Corner)
			{
				const FVector Point(
					(Corner & 1) ? MeshBounds.Max.X : MeshBounds.Min.X,
					(Corner & 2) ? MeshBounds.Max.Y : MeshBounds.Min.Y,
					(Corner & 4) ? MeshBounds.Max.Z : MeshBounds.Min.Z);
				Farthest = FMath::Max(Farthest, FVector::Dist(Location, Point));
			}

			UE_LOG(LogBGOptimize, Log, TEXT("[HLOD] %s (%s) — 맵 끝까지 최대 %.0f m"),
				*It->GetActorLabel(), *ClassName, Farthest / 100.0);
		}
	}

	static FAutoConsoleCommand HLODReportCommand(
		TEXT("BG.Optimize.HLODReport"),
		TEXT("월드 파티션 스트리밍·HLOD 설정과 카메라에서 맵 끝까지의 거리를 출력한다(맵을 바꾸지 않음)."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunHLODReport));

	static FAutoConsoleCommand InstanceCommand(
		TEXT("BG.Optimize.Instance"),
		TEXT("반복 메시를 셀별 HISM 으로 묶는다. 인자 없으면 미리보기, 'apply' 면 적용한다."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunInstance));

	static FAutoConsoleCommand CullDistanceCommand(
		TEXT("BG.Optimize.CullDistance"),
		TEXT("크기별 그리기 거리. 인자 없으면 미리보기, 'apply' 면 Cull Distance Volume 을 만든다."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunCullDistance));

	static FAutoConsoleCommand SmallShadowsCommand(
		TEXT("BG.Optimize.SmallShadows"),
		TEXT("작은 소품 그림자 끄기. 인자 없으면 미리보기, 'apply' 면 적용한다."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&RunSmallShadows));
}
