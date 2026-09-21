#include "Ping/BGPingTypes.h"

#include "BGGameplayTags.h"

DEFINE_LOG_CATEGORY(LogBGPing);

namespace BGPing
{
	static FBGPingDef MakeDef(FGameplayTag PingTag, const TCHAR* Label, bool bNeedsTarget,
		FLinearColor IconColor, FGameplayTag AudioTag, EBGPingWheelDirection Direction)
	{
		FBGPingDef Def;
		Def.PingTag = PingTag;
		Def.Label = FText::FromString(Label);
		Def.bNeedsTarget = bNeedsTarget;
		Def.IconColor = IconColor;
		Def.AudioTag = AudioTag;
		Def.WheelDirection = Direction;
		return Def;
	}

	const TArray<FBGPingDef>& GetDefaultDefs()
	{
		static const TArray<FBGPingDef> Defs = {
			MakeDef(BG::Ping::Attack.GetTag(),  TEXT("공격"), true,
				FLinearColor(0.90f, 0.15f, 0.15f), BG::Audio::Ping::Attack.GetTag(),  EBGPingWheelDirection::Center),
			MakeDef(BG::Ping::Warning.GetTag(), TEXT("주의"), false,
				FLinearColor(0.95f, 0.25f, 0.20f), BG::Audio::Ping::Warning.GetTag(), EBGPingWheelDirection::Up),
			MakeDef(BG::Ping::Suspect.GetTag(), TEXT("의심"), true,
				FLinearColor(0.97f, 0.58f, 0.10f), BG::Audio::Ping::Suspect.GetTag(), EBGPingWheelDirection::Right),
			MakeDef(BG::Ping::Protect.GetTag(), TEXT("보호"), true,
				FLinearColor(0.23f, 0.51f, 0.96f), BG::Audio::Ping::Protect.GetTag(), EBGPingWheelDirection::Down),
			MakeDef(BG::Ping::Heal.GetTag(),    TEXT("치유"), false,
				FLinearColor(0.13f, 0.77f, 0.37f), BG::Audio::Ping::Heal.GetTag(),    EBGPingWheelDirection::Left)
		};
		return Defs;
	}

	const FBGPingDef* FindDefaultDef(FGameplayTag PingTag)
	{
		return GetDefaultDefs().FindByPredicate(
			[PingTag](const FBGPingDef& Def) { return Def.PingTag == PingTag; });
	}

	FGameplayTag ResolvePingTag(const FString& Name)
	{
		for (const FBGPingDef& Def : GetDefaultDefs())
		{
			const FString Full = Def.PingTag.ToString();
			FString Short;
			Full.Split(TEXT("."), nullptr, &Short, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (Name.Equals(Full, ESearchCase::IgnoreCase) || Name.Equals(Short, ESearchCase::IgnoreCase))
			{
				return Def.PingTag;
			}
		}
		return FGameplayTag();
	}
}
