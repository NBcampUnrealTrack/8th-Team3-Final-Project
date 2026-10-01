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
				FLinearColor(FColor::FromHex(TEXT("EF6268"))), BG::Audio::Ping::Attack.GetTag(),  EBGPingWheelDirection::Center),
			MakeDef(BG::Ping::Warning.GetTag(), TEXT("경고"), false,
				FLinearColor(FColor::FromHex(TEXT("F5A623"))), BG::Audio::Ping::Warning.GetTag(), EBGPingWheelDirection::Up),
			MakeDef(BG::Ping::Suspect.GetTag(), TEXT("의심"), true,
				FLinearColor(FColor::FromHex(TEXT("F4D35E"))), BG::Audio::Ping::Suspect.GetTag(), EBGPingWheelDirection::Right),
			MakeDef(BG::Ping::Protect.GetTag(), TEXT("보호"), true,
				FLinearColor(FColor::FromHex(TEXT("79B7F5"))), BG::Audio::Ping::Protect.GetTag(), EBGPingWheelDirection::Down),
			MakeDef(BG::Ping::Heal.GetTag(),    TEXT("치유"), false,
				FLinearColor(FColor::FromHex(TEXT("55D69A"))), BG::Audio::Ping::Heal.GetTag(),    EBGPingWheelDirection::Left)
		};
		return Defs;
	}

	FLinearColor GetSeatColor(int32 Seat)
	{
		//	좌석 8칸 모두 다른 색. 종류색 5개와 CIE76 ΔE 30 이상 떨어지게 골랐다(round5/palette-table.md).
		static const FLinearColor Palette[8] = {
			FLinearColor(FColor::FromHex(TEXT("FF5CCB"))), FLinearColor(FColor::FromHex(TEXT("3D7BFF"))),
			FLinearColor(FColor::FromHex(TEXT("2FD9D9"))), FLinearColor(FColor::FromHex(TEXT("C6F03A"))),
			FLinearColor(FColor::FromHex(TEXT("B070FF"))), FLinearColor(FColor::FromHex(TEXT("E6E6E6"))),
			FLinearColor(FColor::FromHex(TEXT("FF3030"))), FLinearColor(FColor::FromHex(TEXT("2BD84F")))
		};
		if (Seat >= 0 && Seat < static_cast<int32>(UE_ARRAY_COUNT(Palette)))
		{
			return Palette[Seat];
		}

		static bool bWarned = false;
		if (!bWarned && Seat != INDEX_NONE)
		{
			bWarned = true;
			UE_LOG(LogBGPing, Warning, TEXT("[BGPing] 좌석 %d 는 팔레트 범위 밖 — 회색으로 표시"), Seat);
		}
		return FLinearColor(0.45f, 0.45f, 0.45f);
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
