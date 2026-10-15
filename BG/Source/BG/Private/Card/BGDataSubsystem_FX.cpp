#include "Card/BGDataSubsystem.h"

#include "Debug/BGCardLog.h"
#include "FX/BGCardNiagaraTypes.h"
#include "FX/BGFXResourceLibrary.h"
#include "FX/BGPresentationDataAsset.h"
#include "FX/BGStatusFXTypes.h"
#include "Misc/App.h"
#include "NiagaraSystem.h"

void UBGDataSubsystem::PreloadPresentationFX()
{
    if (!FApp::CanEverRender() || IsRunningDedicatedServer()) return;

    TSet<FSoftObjectPath> Paths;
    const auto Add = [&Paths](const TSoftObjectPtr<UNiagaraSystem>& Effect)
    {
        if (!Effect.IsNull()) Paths.Add(Effect.ToSoftObjectPath());
    };
    if (CardFXTable)
    {
        CardFXTable->ForeachRow<FBGCardFXRow>(TEXT("PreloadCardFX"), [&](const FName&, const FBGCardFXRow& Row)
        {
            Add(Row.PrimaryEffect);
            Add(Row.FailureEffect);
            Add(Row.CriticalFailureEffect);
            Add(Row.SuccessEffect);
            Add(Row.CriticalSuccessEffect);
        });
    }
    if (StatusFXTable)
    {
        StatusFXTable->ForeachRow<FBGStatusFXRow>(TEXT("PreloadStatusFX"), [&](const FName&, const FBGStatusFXRow& Row)
        {
            Add(Row.Effect);
        });
    }
    if (CharacterTable)
    {
        CharacterTable->ForeachRow<FBGCharacterRow>(TEXT("PreloadAttackFX"), [&](const FName&, const FBGCharacterRow& Row)
        {
            Add(Row.AttackFX.AttackEffect);
            Add(Row.AttackFX.DamageSuccessEffect);
            Add(Row.AttackFX.DamageCriticalEffect);
        });
    }
    if (PresentationData)
    {
        Add(PresentationData->FailureEffect);
        Add(PresentationData->CriticalFailureEffect);
        Add(PresentationData->BasicTargetSelectionEffect);
        Add(PresentationData->MagicTargetSelectionEffect);
        Add(PresentationData->PhysicalTargetSelectionEffect);
    }
    const double Started = FPlatformTime::Seconds();
    for (const FSoftObjectPath& Path : Paths)
    {
        if (UNiagaraSystem* System = BGFX::LoadSystemReady(TSoftObjectPtr<UNiagaraSystem>(Path)))
        {
            PreloadedPresentationFX.Add(System);
        }
        else
        {
            UE_LOG(LogBGCard, Warning, TEXT("[FXReady] Failed to preload %s"), *Path.ToString());
        }
    }
    UE_LOG(LogBGCard, Log, TEXT("[FXReady] Preloaded %d/%d systems in %.2fs"),
        PreloadedPresentationFX.Num(), Paths.Num(), FPlatformTime::Seconds() - Started);
}
