#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BGEndingToolsLibrary.generated.h"
class AStaticMeshActor;
class AChaosCacheManager;
class UGeometryCollectionComponent;
class UChaosCacheCollection;
class UWidgetBlueprint;
class UWidget;
class UUserWidget;
UCLASS()
class BGENDINGTOOLS_API UBGEndingToolsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static bool RenderWidgetPreview(TSubclassOf<UUserWidget> WidgetClass, const FString& RowData, const FString& OutputPath);

    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static FString InspectCache(UChaosCacheCollection* Cache);
    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static void ConfigureCache(AChaosCacheManager* Manager, UGeometryCollectionComponent* Component, UChaosCacheCollection* Cache);
    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static void RepairWidgetGuids(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static void SetWidgetRoot(UWidgetBlueprint* Blueprint, UWidget* Root);

    /** Fractures selected castle pieces and records six seconds of Chaos simulation in an isolated world. */
    UFUNCTION(BlueprintCallable, Category="BG|EndingTools")
    static FString BakeCastle(const TArray<AStaticMeshActor*>& Sources, const FString& CollectionPath, const FString& CachePath);
};
