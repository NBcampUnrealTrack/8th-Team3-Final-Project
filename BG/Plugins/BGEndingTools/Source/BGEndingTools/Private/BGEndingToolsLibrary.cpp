#include "BGEndingToolsLibrary.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/WorldSettings.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionConversion.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"
#include "FractureEngineFracturing.h"
#include "Chaos/CacheManagerActor.h"
#include "Chaos/CacheCollection.h"
#include "Chaos/ChaosCache.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "AssetCompilingManager.h"
#include "CoreGlobals.h"
#include "Components/BoxComponent.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, BGEndingTools)

static bool SaveEndingAsset(UObject* Asset)
{
    Asset->MarkPackageDirty();
    FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone; Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Asset->GetPackage(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetPackage()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}

FString UBGEndingToolsLibrary::BakeCastle(const TArray<AStaticMeshActor*>& Sources,const FString& CollectionPath,const FString& CachePath)
{
    if (Sources.IsEmpty()) return TEXT("ERROR: no meshes selected");
    if (FindObject<UObject>(nullptr,*(CollectionPath+TEXT(".")+FPackageName::GetShortName(CollectionPath)))) return TEXT("ERROR: asset already loaded");
    UGeometryCollection* Asset=NewObject<UGeometryCollection>(CreatePackage(*CollectionPath),*FPackageName::GetShortName(CollectionPath),RF_Public|RF_Standalone);
    FBox Bounds(ForceInit);
    for (AStaticMeshActor* Source:Sources)
    {
        if (!Source || !Source->GetStaticMeshComponent()->GetStaticMesh()) continue;
        Bounds+=Source->GetComponentsBoundingBox();
        FGeometryCollectionConversion::AppendStaticMesh(Source->GetStaticMeshComponent()->GetStaticMesh(),Source->GetStaticMeshComponent(),Source->GetActorTransform(),Asset,false);
    }
    Asset->ReindexMaterialSections();
    TSharedPtr<FGeometryCollection,ESPMode::ThreadSafe> GC=Asset->GetGeometryCollection();
    FDataflowTransformSelection Selection; Selection.Initialize(GC->Transform.Num(),true);
    FUniformFractureSettings Settings{};
    Settings.Transform=FTransform::Identity;
    Settings.MinVoronoiSites=14;Settings.MaxVoronoiSites=18;
    Settings.InternalMaterialID=-1;Settings.RandomSeed=290926;
    Settings.ChanceToFracture=1.f;Settings.GroupFracture=false;Settings.SplitIslands=true;
    Settings.Grout=1.f;Settings.NoiseSettings.Amplitude=0.f;Settings.NoiseSettings.PointSpacing=100.f;
    Settings.AddSamplesForCollision=false;Settings.CollisionSampleSpacing=50.f;
    FFractureEngineFracturing::UniformFracture(*GC,Selection,Settings);
    // Bake independent rigid fragments, without the source-mesh cluster parents.
    TArray<FTransform> FracturedGlobal;
    GeometryCollectionAlgo::GlobalMatrices(GC->Transform,GC->Parent,FracturedGlobal);
    TArray<int32> Clusters;
    for(int32 I=0;I<GC->Transform.Num();++I)
    {
        GC->Transform[I]=FTransform3f(FracturedGlobal[I]);GC->Parent[I]=INDEX_NONE;GC->Children[I].Reset();
        if(GC->SimulationType[I]!=FGeometryCollection::ESimulationTypes::FST_Rigid)Clusters.Add(I);
    }
    GC->RemoveElements(FGeometryCollection::TransformGroup,Clusters);
    Asset->EnableClustering=false;
    Chaos::Facades::FCollectionAnchoringFacade Anchors(*GC);
    Anchors.AddAnchoredAttribute();
    TArray<FTransform> Global;
    GeometryCollectionAlgo::GlobalMatrices(GC->Transform,GC->Parent,Global);
    double LowestCenter=DBL_MAX;
    for (int32 I=0; I<GC->Transform.Num(); ++I)
    {
        const int32 G=GC->TransformToGeometryIndex[I];
        if(G!=INDEX_NONE && GC->SimulationType[I]==FGeometryCollection::ESimulationTypes::FST_Rigid)
            LowestCenter=FMath::Min(LowestCenter,Global[I].TransformPosition(FVector(GC->BoundingBox[G].GetCenter())).Z);
    }
    int32 AnchoredCount=0, RigidCount=0;
    for (int32 I=0;I<GC->Transform.Num();++I)
    {
        if (GC->SimulationType[I] != FGeometryCollection::ESimulationTypes::FST_Rigid) continue;
        ++RigidCount;
        const int32 G=GC->TransformToGeometryIndex[I];
        if (G!=INDEX_NONE && Global[I].TransformPosition(FVector(GC->BoundingBox[G].GetCenter())).Z < FMath::Max(LowestCenter+100.f,1100.0))
        { Anchors.SetAnchored(I,true); Anchors.SetInitialDynamicState(I,Chaos::EObjectStateType::Kinematic); ++AnchoredCount; }
    }
    // Shrunk convex fragments avoid initial overlap impulses between adjacent fractured walls.
    // This is an editor bake only; clients replay the recorded transforms.
    for (FGeometryCollectionSizeSpecificData& Size : Asset->SizeSpecificData)
        for (FGeometryCollectionCollisionTypeData& Shape : Size.CollisionShapes)
        {
            Shape.ImplicitType = EImplicitTypeEnum::Chaos_Implicit_Convex;
            Shape.CollisionObjectReductionPercentage = 5.f;
            Shape.CollisionType = ECollisionTypeEnum::Chaos_Volumetric;
        }
    Asset->UpdateGeometryDependentProperties();Asset->InvalidateCollection();Asset->CreateSimulationData();
    FAssetRegistryModule::AssetCreated(Asset);
    if (!SaveEndingAsset(Asset)) return TEXT("ERROR: collection save failed");
    UChaosCacheCollection* Cache=NewObject<UChaosCacheCollection>(CreatePackage(*CachePath),*FPackageName::GetShortName(CachePath),RF_Public|RF_Standalone);
    FAssetRegistryModule::AssetCreated(Cache);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,FName(TEXT("BGEndingBake")));
    FWorldContext& Context=GEngine->CreateNewWorldContext(EWorldType::Game);Context.SetCurrentWorld(World);World->bShouldSimulatePhysics=true;
    AActor* Ground=World->SpawnActor<AActor>();
    UBoxComponent* GroundBox=NewObject<UBoxComponent>(Ground);
    Ground->SetRootComponent(GroundBox);Ground->AddInstanceComponent(GroundBox);
    GroundBox->SetMobility(EComponentMobility::Movable);GroundBox->SetBoxExtent(FVector(9000,9000,500));
    GroundBox->SetCollisionProfileName(TEXT("BlockAll"));GroundBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    GroundBox->RegisterComponent();Ground->SetActorLocation(FVector(Bounds.GetCenter().X,Bounds.GetCenter().Y,FMath::Max(Bounds.Min.Z-10.f,950.0)-500.f));
    AGeometryCollectionActor* Actor=World->SpawnActor<AGeometryCollectionActor>();
    UGeometryCollectionComponent* Component=Actor->GetGeometryCollectionComponent();
    Component->EnableClustering=false;Component->SetRestCollection(Asset,false);Component->RecreatePhysicsState();
    Component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Component->SetCollisionResponseToAllChannels(ECR_Block);Component->SetUseCCD(true);Component->SetSimulatePhysics(true);
    AChaosCacheManager* Manager=World->SpawnActor<AChaosCacheManager>();
    Manager->CacheCollection=Cache;Manager->CacheMode=ECacheMode::Record;Manager->StartMode=EStartMode::Timed;Manager->bStartOnBeginPlay=false;
    Manager->FindOrAddObservedComponent(Component,TEXT("CastleUpper"));
    FAssetCompilingManager::Get().FinishAllCompilation();
    World->InitializeActorsForPlay(FURL());World->GetWorldSettings()->NotifyBeginPlay();World->BeginPlay();
    Manager->Start();
    for (int32 Frame=0;Frame<720;++Frame)
    { ++GFrameCounter;World->Tick(LEVELTICK_All,1.f/120.f); }
    Manager->Stop();Cache->FlushAllCacheWrites();
    const float Duration=Cache->GetMaxDuration();const uint32 Frames=Cache->GetMaxNumFrames();
    const FString Motion=InspectCache(Cache);
    const bool bSaved=Duration>5.f && Frames>100 && !Motion.Contains(TEXT("moving=0 ")) && SaveEndingAsset(Cache);
    World->EndPlay(EEndPlayReason::Quit);
    for (AActor* A : {static_cast<AActor*>(Manager), static_cast<AActor*>(Actor), static_cast<AActor*>(Ground)}) A->Destroy();
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    return FString::Printf(TEXT("%s rigid=%d anchored=%d cache_frames=%u duration=%.3f"),bSaved?TEXT("OK"):TEXT("ERROR: empty recording"),RigidCount,AnchoredCount,Frames,Duration)+TEXT(" ")+Motion;
}

FString UBGEndingToolsLibrary::InspectCache(UChaosCacheCollection* Cache)
{
    int32 Tracks=0,Moving=0;double MaxDistance=0;FString Sample;
    if(!Cache)return TEXT("ERROR null cache");
    for(UChaosCache* C:Cache->GetCaches())for(const FPerParticleCacheData& P:C->ParticleTracks)
    {
        ++Tracks;const auto& Keys=P.TransformData.RawTransformTrack.PosKeys;
        if(Keys.Num()>1){if(Tracks<=3)Sample+=FString::Printf(TEXT(" [%s -> %s]"),*Keys[0].ToString(),*Keys.Last().ToString());double D=(Keys.Last()-Keys[0]).Size();MaxDistance=FMath::Max(MaxDistance,D);if(D>5)++Moving;}
    }
    return FString::Printf(TEXT("tracks=%d moving=%d max_displacement_cm=%.2f frames=%u duration=%.3f"),Tracks,Moving,MaxDistance,Cache->GetMaxNumFrames(),Cache->GetMaxDuration())+Sample;
}
void UBGEndingToolsLibrary::ConfigureCache(AChaosCacheManager* Manager,UGeometryCollectionComponent* Component,UChaosCacheCollection* Cache)
{
    Manager->Modify();Component->Modify();Manager->CacheCollection=Cache;Manager->CacheMode=ECacheMode::None;Manager->StartMode=EStartMode::Timed;Manager->bStartOnBeginPlay=false;
    Manager->FindOrAddObservedComponent(Component,TEXT("CastleUpper"));
    Component->SetSimulatePhysics(false);
}
void UBGEndingToolsLibrary::RepairWidgetGuids(UWidgetBlueprint* Blueprint)
{
    Blueprint->Modify();Blueprint->WidgetTree->ForEachWidget([Blueprint](UWidget* W){if(!Blueprint->WidgetVariableNameToGuidMap.Contains(W->GetFName()))Blueprint->WidgetVariableNameToGuidMap.Add(W->GetFName(),FGuid::NewGuid());});
}

void UBGEndingToolsLibrary::SetWidgetRoot(UWidgetBlueprint* Blueprint,UWidget* Root){Blueprint->Modify();Blueprint->WidgetTree->Modify();Blueprint->WidgetTree->RootWidget=Root;}




#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ImageUtils.h"
#include "Serialization/BufferArchive.h"
#include "Misc/FileHelper.h"
#include "UObject/StructOnScope.h"
#include "RenderingThread.h"

bool UBGEndingToolsLibrary::RenderWidgetPreview(TSubclassOf<UUserWidget> WidgetClass,const FString& RowData,const FString& OutputPath)
{
    if(!WidgetClass)return false;
    UUserWidget* Widget=NewObject<UUserWidget>(GetTransientPackage(),WidgetClass);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);Widget->Initialize();
    TSharedRef<SWidget> SlateWidget=Widget->TakeWidget();
    if(UFunction* Function=Widget->FindFunction(TEXT("OnResultReady")))
    {
        FStructOnScope Params(Function);
        FArrayProperty* Rows=FindFProperty<FArrayProperty>(Function,TEXT("InRows"));
        if(!Rows || !Rows->ImportText_Direct(*RowData,Rows->ContainerPtrToValuePtr<void>(Params.GetStructMemory()),Widget,PPF_None))return false;
        Widget->ProcessEvent(Function,Params.GetStructMemory());
    }
    if(UTextBlock* Title=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_ResultTitle"))))Title->SetText(FText::FromString(TEXT("보안관 진영 승리")));
    if(UTextBlock* Reason=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("Text_ResultReason"))))Reason->SetText(FText::FromString(TEXT("무법자와 배신자가 모두 쓰러졌습니다.")));
    // Designer previews ignore collapsed state; the result-only capture hides the victory banner explicitly.
    if(UWidget* Banner=Widget->GetWidgetFromName(TEXT("Banner_Victory")))Banner->SetRenderOpacity(0.f);
    FWidgetRenderer Renderer(true,true);
    UTextureRenderTarget2D* Target=Renderer.DrawWidget(SlateWidget,FVector2D(1600,900));
    FlushRenderingCommands();
    FBufferArchive Buffer;
    bool Saved=FImageUtils::ExportRenderTarget2DAsPNG(Target,Buffer) && FFileHelper::SaveArrayToFile(Buffer,*OutputPath);
    Widget->ReleaseSlateResources(true);
    return Saved;
}
