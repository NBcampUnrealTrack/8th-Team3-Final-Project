#include "BGGameplayTags.h"

// =====================================================================================
//  BANG! 게임태그 정의 (선언은 Public/Game/BGGameplayTags.h)
//  변수 경로(BG::Card::Id::Bang)와 문자열("Card.Id.Bang")을 1:1로 유지한다.
// =====================================================================================

namespace BG
{
	namespace Audio
	{
		namespace UI
		{
			namespace MatchResult
			{
				UE_DEFINE_GAMEPLAY_TAG(Victory, "Audio.UI.MatchResult.Victory")
				UE_DEFINE_GAMEPLAY_TAG(Defeat, "Audio.UI.MatchResult.Defeat")
			}

			namespace Chat
			{
				UE_DEFINE_GAMEPLAY_TAG(Typing, "Audio.UI.Chat.Typing")
				UE_DEFINE_GAMEPLAY_TAG(Message, "Audio.UI.Chat.Message")
			}

			namespace Button
			{
				UE_DEFINE_GAMEPLAY_TAG(HoverBegin,    "Audio.UI.Button.Hover.Begin")
				UE_DEFINE_GAMEPLAY_TAG(HoverEnd,      "Audio.UI.Button.Hover.End")
				UE_DEFINE_GAMEPLAY_TAG(PressBegin,    "Audio.UI.Button.Press.Begin")
				UE_DEFINE_GAMEPLAY_TAG(PressEnd,      "Audio.UI.Button.Press.End")
				UE_DEFINE_GAMEPLAY_TAG(Confirm,       "Audio.UI.Button.Confirm")
				UE_DEFINE_GAMEPLAY_TAG(Navigation,    "Audio.UI.Button.Navigation")
				UE_DEFINE_GAMEPLAY_TAG(Selection,     "Audio.UI.Button.Selection")
				UE_DEFINE_GAMEPLAY_TAG(Exit,          "Audio.UI.Button.Exit")
			}

			namespace Card
			{
				UE_DEFINE_GAMEPLAY_TAG(Draw, "Audio.UI.Card.Draw")
				UE_DEFINE_GAMEPLAY_TAG(HoverBegin, "Audio.UI.Card.Hover.Begin")
				UE_DEFINE_GAMEPLAY_TAG(HoverEnd,   "Audio.UI.Card.Hover.End")
				UE_DEFINE_GAMEPLAY_TAG(DragBegin,  "Audio.UI.Card.Drag.Begin")
				UE_DEFINE_GAMEPLAY_TAG(DragEnd,    "Audio.UI.Card.Drag.End")
			}

			namespace ComboBox
			{
				UE_DEFINE_GAMEPLAY_TAG(HoverBegin, "Audio.UI.ComboBox.Hover.Begin")
				UE_DEFINE_GAMEPLAY_TAG(HoverEnd,   "Audio.UI.ComboBox.Hover.End")
				UE_DEFINE_GAMEPLAY_TAG(Open,       "Audio.UI.ComboBox.Open")
				UE_DEFINE_GAMEPLAY_TAG(Select,     "Audio.UI.ComboBox.Select")
			}

			namespace Slider
			{
				UE_DEFINE_GAMEPLAY_TAG(HoverBegin,  "Audio.UI.Slider.Hover.Begin")
				UE_DEFINE_GAMEPLAY_TAG(HoverEnd,    "Audio.UI.Slider.Hover.End")
				UE_DEFINE_GAMEPLAY_TAG(CaptureBegin,"Audio.UI.Slider.Capture.Begin")
				UE_DEFINE_GAMEPLAY_TAG(CaptureEnd,  "Audio.UI.Slider.Capture.End")
			}

			namespace RollGauge
			{
				UE_DEFINE_GAMEPLAY_TAG(LightStep, "Audio.UI.RollGauge.LightStep")

				namespace Result
				{
					UE_DEFINE_GAMEPLAY_TAG(CriticalFailure, "Audio.UI.RollGauge.Result.CriticalFailure")
					UE_DEFINE_GAMEPLAY_TAG(Failure,         "Audio.UI.RollGauge.Result.Failure")
					UE_DEFINE_GAMEPLAY_TAG(Success,         "Audio.UI.RollGauge.Result.Success")
					UE_DEFINE_GAMEPLAY_TAG(CriticalSuccess, "Audio.UI.RollGauge.Result.CriticalSuccess")
				}
			}
		}

		namespace System
		{
			UE_DEFINE_GAMEPLAY_TAG(TurnStart,   "Audio.System.Turn.Start")
			UE_DEFINE_GAMEPLAY_TAG(TurnCountdown, "Audio.System.Turn.Countdown")
			UE_DEFINE_GAMEPLAY_TAG(TurnEnd,     "Audio.System.Turn.End")
			UE_DEFINE_GAMEPLAY_TAG(RoleReveal,  "Audio.System.RoleReveal")
			UE_DEFINE_GAMEPLAY_TAG(Victory,     "Audio.System.Victory")
			UE_DEFINE_GAMEPLAY_TAG(Defeat,      "Audio.System.Defeat")
		}

		namespace Music
		{
			UE_DEFINE_GAMEPLAY_TAG(MainMenu, "Audio.Music.MainMenu")
			UE_DEFINE_GAMEPLAY_TAG(Gameplay, "Audio.Music.Gameplay")
			UE_DEFINE_GAMEPLAY_TAG(Ending, "Audio.Music.Ending")
		}

		namespace Ping
		{
			UE_DEFINE_GAMEPLAY_TAG(Attack,  "Audio.Ping.Attack")
			UE_DEFINE_GAMEPLAY_TAG(Warning, "Audio.Ping.Warning")
			UE_DEFINE_GAMEPLAY_TAG(Suspect, "Audio.Ping.Suspect")
			UE_DEFINE_GAMEPLAY_TAG(Protect, "Audio.Ping.Protect")
			UE_DEFINE_GAMEPLAY_TAG(Heal,    "Audio.Ping.Heal")
		}
	}

	// 나이아가라 시스템 및 효과/연출용 게임플레이 큐
	namespace GameplayCue
	{
		namespace Card
		{
			UE_DEFINE_GAMEPLAY_TAG(Primary,   "GameplayCue.Card.Primary")
			UE_DEFINE_GAMEPLAY_TAG(Secondary, "GameplayCue.Card.Secondary")
		}
	}

	namespace Card
	{
		namespace Type
		{
			UE_DEFINE_GAMEPLAY_TAG(Brown, "Card.Type.Brown")
			UE_DEFINE_GAMEPLAY_TAG(Blue,  "Card.Type.Blue")
		}

		namespace Id
		{
			UE_DEFINE_GAMEPLAY_TAG(Bang,         "Card.Id.Bang")
			UE_DEFINE_GAMEPLAY_TAG(Missed,       "Card.Id.Missed")
			UE_DEFINE_GAMEPLAY_TAG(Beer,         "Card.Id.Beer")
			UE_DEFINE_GAMEPLAY_TAG(Saloon,       "Card.Id.Saloon")
			UE_DEFINE_GAMEPLAY_TAG(Stagecoach,   "Card.Id.Stagecoach")
			UE_DEFINE_GAMEPLAY_TAG(WellsFargo,   "Card.Id.WellsFargo")
			UE_DEFINE_GAMEPLAY_TAG(CatBalou,     "Card.Id.CatBalou")
			UE_DEFINE_GAMEPLAY_TAG(Panic,        "Card.Id.Panic")
			UE_DEFINE_GAMEPLAY_TAG(Duel,         "Card.Id.Duel")
			UE_DEFINE_GAMEPLAY_TAG(GeneralStore, "Card.Id.GeneralStore")
			UE_DEFINE_GAMEPLAY_TAG(Indians,      "Card.Id.Indians")
			UE_DEFINE_GAMEPLAY_TAG(Gatling,      "Card.Id.Gatling")
			UE_DEFINE_GAMEPLAY_TAG(Jail,         "Card.Id.Jail")
			UE_DEFINE_GAMEPLAY_TAG(Dynamite,     "Card.Id.Dynamite")
			UE_DEFINE_GAMEPLAY_TAG(Barrel,       "Card.Id.Barrel")
			UE_DEFINE_GAMEPLAY_TAG(Mustang,      "Card.Id.Mustang")
			UE_DEFINE_GAMEPLAY_TAG(Scope,        "Card.Id.Scope")
			UE_DEFINE_GAMEPLAY_TAG(Volcanic,     "Card.Id.Volcanic")
			UE_DEFINE_GAMEPLAY_TAG(Schofield,    "Card.Id.Schofield")
			UE_DEFINE_GAMEPLAY_TAG(Remington,    "Card.Id.Remington")
			UE_DEFINE_GAMEPLAY_TAG(RevCarabine,  "Card.Id.RevCarabine")
			UE_DEFINE_GAMEPLAY_TAG(Winchester,   "Card.Id.Winchester")

			// ── BAAM 신규 카드 (Docs/Additional-Card-List.txt) ──
			//  공격
			UE_DEFINE_GAMEPLAY_TAG(QuickStrike,    "Card.Id.QuickStrike")
			UE_DEFINE_GAMEPLAY_TAG(ExposeWeakness, "Card.Id.ExposeWeakness")
			UE_DEFINE_GAMEPLAY_TAG(Trap,           "Card.Id.Trap")

			//  회복
			UE_DEFINE_GAMEPLAY_TAG(Prepare,        "Card.Id.Prepare")
			UE_DEFINE_GAMEPLAY_TAG(Rest,           "Card.Id.Rest")
			UE_DEFINE_GAMEPLAY_TAG(Brace,          "Card.Id.Brace")

			//  장비(파란 카드)
			UE_DEFINE_GAMEPLAY_TAG(DwarfGloves,    "Card.Id.DwarfGloves")
			UE_DEFINE_GAMEPLAY_TAG(FairyArmor,     "Card.Id.FairyArmor")
			UE_DEFINE_GAMEPLAY_TAG(WitchCharm,     "Card.Id.WitchCharm")

			//  일회용 (덱에 안 들어간다)
			UE_DEFINE_GAMEPLAY_TAG(ShadowStrike,   "Card.Id.ShadowStrike")
		}

		namespace Trait
		{
			UE_DEFINE_GAMEPLAY_TAG(Weapon,          "Card.Trait.Weapon")
			UE_DEFINE_GAMEPLAY_TAG(NeedsTargetSeat, "Card.Trait.NeedsTargetSeat")
			UE_DEFINE_GAMEPLAY_TAG(Reaction,        "Card.Trait.Reaction")
			UE_DEFINE_GAMEPLAY_TAG(RangeLimited,    "Card.Trait.RangeLimited")
			UE_DEFINE_GAMEPLAY_TAG(Attack,          "Card.Trait.Attack")
			UE_DEFINE_GAMEPLAY_TAG(Cinematic,       "Card.Trait.Cinematic")
			UE_DEFINE_GAMEPLAY_TAG(Ephemeral,       "Card.Trait.Ephemeral")
		}
	}

	namespace Ability
	{
		UE_DEFINE_GAMEPLAY_TAG(Bang,           "Ability.Bang")
		UE_DEFINE_GAMEPLAY_TAG(Missed,         "Ability.Missed")
		UE_DEFINE_GAMEPLAY_TAG(Heal,           "Ability.Heal")
		UE_DEFINE_GAMEPLAY_TAG(DrawN,          "Ability.DrawN")
		UE_DEFINE_GAMEPLAY_TAG(StealOrDiscard, "Ability.StealOrDiscard")
		UE_DEFINE_GAMEPLAY_TAG(AreaBang,       "Ability.AreaBang")
		UE_DEFINE_GAMEPLAY_TAG(Duel,           "Ability.Duel")
		UE_DEFINE_GAMEPLAY_TAG(Saloon,         "Ability.Saloon")
		UE_DEFINE_GAMEPLAY_TAG(GeneralStore,   "Ability.GeneralStore")
	}

	namespace State
	{
		UE_DEFINE_GAMEPLAY_TAG(Dead,        "State.Dead")
		UE_DEFINE_GAMEPLAY_TAG(Jailed,      "State.Jailed")
		UE_DEFINE_GAMEPLAY_TAG(HasDynamite, "State.HasDynamite")
		UE_DEFINE_GAMEPLAY_TAG(Boss,        "State.Boss")

		namespace Turn
		{
			UE_DEFINE_GAMEPLAY_TAG(Active,     "State.Turn.Active")
			UE_DEFINE_GAMEPLAY_TAG(BangPlayed, "State.Turn.BangPlayed")
		}
	}

	namespace Phase
	{
		UE_DEFINE_GAMEPLAY_TAG(Lobby,     "Phase.Lobby")
		UE_DEFINE_GAMEPLAY_TAG(Intro,     "Phase.Intro")
		UE_DEFINE_GAMEPLAY_TAG(TurnStart, "Phase.TurnStart")
		UE_DEFINE_GAMEPLAY_TAG(Draw,      "Phase.Draw")
		UE_DEFINE_GAMEPLAY_TAG(Play,      "Phase.Play")
		UE_DEFINE_GAMEPLAY_TAG(Discard,   "Phase.Discard")
		UE_DEFINE_GAMEPLAY_TAG(GameOver,  "Phase.GameOver")
	}

	namespace Response
	{
		namespace Allow
		{
			UE_DEFINE_GAMEPLAY_TAG(Missed,         "Response.Allow.Missed")
			UE_DEFINE_GAMEPLAY_TAG(Bang,           "Response.Allow.Bang")
			UE_DEFINE_GAMEPLAY_TAG(PickRevealed,   "Response.Allow.PickRevealed")
			UE_DEFINE_GAMEPLAY_TAG(PickFromPlayer, "Response.Allow.PickFromPlayer")
		}
	}

	namespace Status
	{
		namespace NextCard
		{
			UE_DEFINE_GAMEPLAY_TAG(ForceSuccess,         "Status.NextCard.ForceSuccess")
			UE_DEFINE_GAMEPLAY_TAG(ForceCriticalSuccess, "Status.NextCard.ForceCriticalSuccess")
			UE_DEFINE_GAMEPLAY_TAG(ForceFailure,         "Status.NextCard.ForceFailure")
			UE_DEFINE_GAMEPLAY_TAG(ForceCriticalFailure, "Status.NextCard.ForceCriticalFailure")
			UE_DEFINE_GAMEPLAY_TAG(KeepCardUse,          "Status.NextCard.KeepCardUse")
		}

		namespace NextAttack
		{
			UE_DEFINE_GAMEPLAY_TAG(DamageBonus,          "Status.NextAttack.DamageBonus")
		}
	}

	namespace Resolution
	{
		UE_DEFINE_GAMEPLAY_TAG(CriticalFailure, "Resolution.CriticalFailure")
		UE_DEFINE_GAMEPLAY_TAG(Failure,         "Resolution.Failure")
		UE_DEFINE_GAMEPLAY_TAG(Success,         "Resolution.Success")
		UE_DEFINE_GAMEPLAY_TAG(CriticalSuccess, "Resolution.CriticalSuccess")
	}

	namespace Role
	{
		UE_DEFINE_GAMEPLAY_TAG(Sheriff,  "Role.Sheriff")
		UE_DEFINE_GAMEPLAY_TAG(Deputy,   "Role.Deputy")
		UE_DEFINE_GAMEPLAY_TAG(Outlaw,   "Role.Outlaw")
		UE_DEFINE_GAMEPLAY_TAG(Renegade, "Role.Renegade")
	}

	namespace Character
	{
		namespace Job
		{
			UE_DEFINE_GAMEPLAY_TAG(Knight,    "Character.Job.Knight")
			UE_DEFINE_GAMEPLAY_TAG(Mercenary, "Character.Job.Mercenary")
			UE_DEFINE_GAMEPLAY_TAG(Hunter,    "Character.Job.Hunter")
			UE_DEFINE_GAMEPLAY_TAG(Scout,     "Character.Job.Scout")
			UE_DEFINE_GAMEPLAY_TAG(Priest,    "Character.Job.Priest")
			UE_DEFINE_GAMEPLAY_TAG(Healer,    "Character.Job.Healer")
			UE_DEFINE_GAMEPLAY_TAG(Mage,      "Character.Job.Mage")
			UE_DEFINE_GAMEPLAY_TAG(Diviner,   "Character.Job.Diviner")
			UE_DEFINE_GAMEPLAY_TAG(Monk,      "Character.Job.Monk")
			UE_DEFINE_GAMEPLAY_TAG(Brawler,   "Character.Job.Brawler")
			UE_DEFINE_GAMEPLAY_TAG(Thief,     "Character.Job.Thief")
			UE_DEFINE_GAMEPLAY_TAG(Assassin,  "Character.Job.Assassin")
		}
	}

	namespace SetByCaller
	{
		UE_DEFINE_GAMEPLAY_TAG(Damage, "SetByCaller.Damage")
		UE_DEFINE_GAMEPLAY_TAG(Heal,   "SetByCaller.Heal")
	}

	namespace Ping
	{
		UE_DEFINE_GAMEPLAY_TAG(Attack,  "Ping.Attack")
		UE_DEFINE_GAMEPLAY_TAG(Warning, "Ping.Warning")
		UE_DEFINE_GAMEPLAY_TAG(Suspect, "Ping.Suspect")
		UE_DEFINE_GAMEPLAY_TAG(Protect, "Ping.Protect")
		UE_DEFINE_GAMEPLAY_TAG(Heal,    "Ping.Heal")
	}
}
