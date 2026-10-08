#pragma once

#include "NativeGameplayTags.h"

// =====================================================================================
//  BANG! 게임태그 분류 (Prototype-Workflow.md §GameplayTag 분류)
// -------------------------------------------------------------------------------------
//  Team4Project 의 BaseGameplayTags 와 동일한 네이티브 태그 방식.
//    - 선언: 이 헤더에서 UE_DECLARE_GAMEPLAY_TAG_EXTERN
//    - 정의: BGGameplayTags.cpp 에서 UE_DEFINE_GAMEPLAY_TAG(변수, "문자열")
//    - 사용: BG::Card::Id::Bang.GetTag() 처럼 참조
//
//  ⚠️ 문자열이 곧 태그의 정체이며 DataAsset/GE/GA 전반에서 이 문자열로 매칭됩니다.
//     오타는 조용한 버그가 되므로 md 의 taxonomy 를 그대로 옮겼습니다.
// =====================================================================================

namespace BG
{
	// ---------------------------------------------------------------------------------
	//  Audio.* — UI/시스템 2D 사운드 카탈로그. Row 이름도 이 문자열과 동일하다.
	//  Niagara와 Montage에 붙는 공간/동작 사운드는 이 분류에 넣지 않는다.
	// ---------------------------------------------------------------------------------
	namespace Audio
	{
		namespace UI
		{
			namespace MatchResult
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Victory)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defeat)
			}

			namespace Chat
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Typing)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message)
			}

			namespace Button
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverEnd)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(PressBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(PressEnd)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Confirm)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Navigation)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Selection)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Exit)
			}

			namespace Card
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Draw)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Confirm)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverEnd)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(DragBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(DragEnd)
			}

			namespace ComboBox
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverEnd)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Open)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Select)
			}

			namespace Slider
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(HoverEnd)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(CaptureBegin)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(CaptureEnd)
			}

			namespace RollGauge
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(LightStep)

				namespace Result
				{
					UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalFailure)
					UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failure)
					UE_DECLARE_GAMEPLAY_TAG_EXTERN(Success)
					UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalSuccess)
				}
			}
		}

		namespace System
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurnStart)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurnCountdown)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurnEnd)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(RoleReveal)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Victory)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defeat)
		}

		namespace Music
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(MainMenu)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gameplay)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ending)
		}

		// Audio.Ping.* — 핑 종류별 알림음. DT_BGSystemAudio 의 Row 이름과 같다.
		namespace Ping
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Warning)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suspect)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Protect)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal)
		}
	}

	// ---------------------------------------------------------------------------------
	//  Ping.* — 플레이어가 서로에게 보내는 신호 5종. 전원 공개, 사망자 발신 불가.
	// ---------------------------------------------------------------------------------
	namespace Ping
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack)   // 저 사람 공격하자 (좌석 필수)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Warning)  // 주의·경고
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suspect)  // 의심된다 (좌석 필수)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Protect)  // 저 사람 지켜라 (좌석 필수)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal)     // 치유해 줘
	}

	// ---------------------------------------------------------------------------------
	//  GameplayCue.* — ASC AddGameplayCue / RemoveGameplayCue 로 유지되는 카드 연출.
	// ---------------------------------------------------------------------------------
	namespace GameplayCue
	{
		namespace Card
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Secondary)
		}
	}

	// ---------------------------------------------------------------------------------
	//  Card.* — 카드 정의용 태그. DT_BGCard 의 각 Row(FBGCardRow)가 들고 있다.
	// ---------------------------------------------------------------------------------
	namespace Card
	{
		// Card.Type.* — 갈색(즉시 효과) / 파란색(장비, 공개 정보)
		namespace Type
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Brown)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Blue)
		}

		// Card.Id.* — 카드 종류 식별자 (기본판 22종)
		namespace Id
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bang)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Missed)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Beer)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Saloon)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Stagecoach)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(WellsFargo)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(CatBalou)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Panic)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Duel)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(GeneralStore)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Indians)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Gatling)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jail)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dynamite)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Barrel)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mustang)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scope)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Volcanic)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Schofield)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Remington)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(RevCarabine)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Winchester)

			// ── BAAM 신규 카드 (Docs/Additional-Card-List.txt) ──
			//  기본판이 아니라 이 프로젝트에서 새로 만든 카드다(GDD §6.4).
			//  효과는 DT_BGCard 의 OutcomeEffects 에 데이터로 적고,
			//  AbilityByCardId 에서 UGA_BGCardEffects(범용 실행기)로 매핑한다.

			//  공격
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(QuickStrike)     // 속공
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ExposeWeakness)  // 약점 포착
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Trap)            // 함정

			//  회복
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Prepare)         // 준비
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Rest)            // 휴식
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Brace)           // 대비

			//  장비(파란 카드)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(DwarfGloves)     // 드워프 장갑
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(FairyArmor)      // 요정 갑옷
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(WitchCharm)      // 마녀의 부적

			//  일회용 (덱에 들어가지 않고 규칙이 지급한다 — Card.Trait.Ephemeral)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ShadowStrike)    // 그림자 일격 — 암살자가 대성공으로 얻는다
		}
		
		namespace Trait
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon)          // 무기(파란 카드, 1개만 장착)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(NeedsTargetSeat) // 대상 좌석 지정 필요
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Reaction)        // 응답 창에서만 사용 (Missed! 등)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(RangeLimited)    // 사거리(WeaponRange) 제약을 받음

			/**
			 * 공격 카드 — 남을 해치는 것이 주 목적인 카드.
			 *
			 * "대상을 지정한다"(NeedsTargetSeat)와 다르다. 함정은 대상을 고르지만 공격이 아니고,
			 * 광역기는 공격이지만 대상을 고르지 않는다. 그래서 태그를 따로 둔다.
			 *
			 * 이 태그를 읽는 곳 (직업 패시브의 적용 범위를 가른다):
			 *   · 용병   — AttackWeightBonusFailure  (공격 카드만 실패하지 않는다)
			 *   · 무투가 — AttackKeepCardUseChance   (공격 카드만 사용한도를 아낀다)
			 *
			 * ⚠️ DT_BGCard 의 TraitTags 에 넣지 않으면 그 카드는 공격으로 취급되지 않는다.
			 *    태그를 빼도 카드 자체는 멀쩡히 동작하므로 조용히 어긋난다 — BG_DumpDeck 으로 확인할 것.
			 */
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack)

			/**
			 * 일회용 카드 — 덱에서 나오지 않고 규칙이 손패에 직접 만들어 주는 카드.
			 *
			 * 이 태그가 붙은 Row 는 세 가지가 다르다:
			 *   · `BuildDeck` 이 **QuantityOfCard 와 무관하게** 건너뛴다 (덱에 절대 안 들어간다)
			 *   · 사용해도 **카드 사용 한도를 소모하지 않는다**
			 *   · 사용하면 버린 패로 가지 않고 **사라진다** (덱 총량이 늘지 않는다)
			 *
			 * 지급은 `ABGGameState::GrantCards(PS, 태그, 장수)` 하나로 한다. 일회용인지는
			 * 그 함수가 이 태그를 보고 정하므로 **호출자는 일회용 여부를 몰라도 된다** —
			 * PvE 에서 "이 카드 N장 줘" 가 카드 종류에 따라 알아서 갈린다.
			 *
			 * ⚠️ 덱에 없으므로 `BG_DumpDeck` 의 덱 집계에 잡히지 않는다. 대신 같은 명령이
			 *    "덱 밖 카드" 구획에서 따로 검사한다 — 그 구획을 지우면 이 카드군의
			 *    OutcomeEffects 가 비어도 아무도 알려주지 않는다.
			 */
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ephemeral)

			//	사용 시 연출을 재생한다 (아트 기획서 "전투 연출" — 사용자 공격 연출을 확대해
			//	보여주고, 판정 결과에 따라 피해자 연출로 이어진다).
			//
			//	⚠️ 이 태그가 없는 카드는 연출 없이 즉시 해결된다. 판정·효과 적용은 태그와 무관하게
			//	   서버에서 똑같이 일어난다 — 연출은 보여주기만 할 뿐 규칙에 관여하지 않는다.
			//	   (GDD §9.2 "자동 판정은 짧은 연출과 로그를 제공하되 별도의 확인 입력을 요구하지 않는다")
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cinematic)       // 사용 시 연출 재생
		}
	}

	// ---------------------------------------------------------------------------------
	//  Ability.* — GA 의 Asset 태그. 카드별이 아니라 "메커니즘별" 1개 (md §1.1).
	//              개별 카드는 DataAsset 이 GA + 파라미터를 지정.
	// ---------------------------------------------------------------------------------
	namespace Ability
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bang)           // GA_Bang
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Missed)         // GA_Missed (응답 전용)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal)           // GA_Heal (Beer/Saloon)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(DrawN)          // GA_DrawN (역마차/웰스파고)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(StealOrDiscard) // GA_StealOrDiscard (Panic/CatBalou)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(AreaBang)       // GA_AreaBang (개틀링/인디언)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Duel)           // GA_Duel
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Saloon)         // GA_Saloon
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(GeneralStore)   // GA_GeneralStore
	}

	// ---------------------------------------------------------------------------------
	//  State.* — ASC 에 상주하는 상태 태그. GE(Infinite) 또는 룰 상태 머신이 부여.
	// ---------------------------------------------------------------------------------
	namespace State
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Dead)         // 사망 → 좌석 원에서 제외
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Jailed)       // 감옥. 턴 시작 뽑기 판정 대상
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(HasDynamite)  // 다이너마이트 보유
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss)         // PvE 보스 조우에서 지정된 무법자

		// State.Turn.* — 현재 턴 진행 중 플레이어의 임시 상태
		namespace Turn
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Active)     // 지금 이 좌석의 턴
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(BangPlayed) // 이번 턴 BANG! 사용함 (BangLimit 판정)
		}
	}

	// ---------------------------------------------------------------------------------
	//  Phase.* — GameState 의 서버 권위 페이즈 상태 머신 (GAS 아님, md §1.4).
	//            맵 이동 없이 Lobby → ... 로 태그만 전환.
	// ---------------------------------------------------------------------------------
	namespace Phase
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lobby)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Intro)     // 도입 연출 — 판은 열렸지만 아직 첫 턴 전
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(TurnStart) // 감옥/다이너마이트 판정
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Draw)      // DrawCount 장 뽑기
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Play)      // 카드 사용
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Discard)   // 손패 한도(=Health)까지 버리기
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameOver)
	}

	// ---------------------------------------------------------------------------------
	//  Response.Allow.* — 응답 창(Resolution)에서 허용되는 응답 종류 (md §3).
	//  ⚠️ [미구현] 응답 창 시스템 자체가 아직 없다. 이 태그들을 읽는 코드는 비활성 GA 뿐이다
	//     (GA_AreaBang / GA_Duel). 태그만 미리 정의해 둔 상태다.
	// ---------------------------------------------------------------------------------
	namespace Response
	{
		namespace Allow
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Missed)         // BANG!/개틀링에 Missed!
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Bang)           // 인디언/결투에 Bang!
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(PickRevealed)   // 잡화점: 공개된 카드 중 선택
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(PickFromPlayer) // Panic!/Cat Balou: 대상에게서 선택
		}
	}

	// ---------------------------------------------------------------------------------
	//  Status.* — "다음 1회" 대기 상태 (Additional-Card-List 의 약점 포착 / 함정 / 준비 / 대비).
	//
	//    저장은 ABGPlayerState::PendingStatuses 다 (태그 + 수치, 복제됨).
	//    ASC 루즈 태그를 쓰지 않는 이유:
	//      · 루즈 태그는 복제되지 않아 클라 UI 가 상태를 볼 수 없다
	//      · 태그만으로는 수치(약점 포착 +1 / +2)를 담을 수 없다
	//      · 검사 지점이 전부 서버 C++ 이라 GAS 태그 조회의 이점이 없다
	//
	//    ⚠️ 한 번 발동되면 소모된다. 중첩은 허용하지 않는다(이미 가진 상태를 주는 카드는 사용 거부).
	// ---------------------------------------------------------------------------------
	namespace Status
	{
		//  다음에 사용하는 카드 1장의 판정을 강제한다.
		namespace NextCard
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ForceSuccess)          // 준비(성공)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ForceCriticalSuccess)  // 준비(대성공)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ForceFailure)          // 함정(성공)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ForceCriticalFailure)  // 함정(대성공)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(KeepCardUse)           // 대비 — 실패해도 사용한도 유지
		}

		//  다음 공격 1회에 적용된다.
		namespace NextAttack
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageBonus)           // 약점 포착 (수치 = 증가량)
		}
	}

	// ---------------------------------------------------------------------------------
	//  Resolution.* — 4단계 판정 결과 (GDD §4.1 / §9.3).
	//    서버가 판정한 뒤 GameplayEventData 에 실어 GA 로 넘긴다. GA 는 판정하지 않고 실행만 한다.
	// ---------------------------------------------------------------------------------
	namespace Resolution
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalFailure)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Failure)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Success)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(CriticalSuccess)
	}

	// ---------------------------------------------------------------------------------
	//  Role.* — 숨겨진 진영 역할. Sheriff 만 공개, 나머지는 사망 시 공개 (md §0/§1.3).
	// ---------------------------------------------------------------------------------
	namespace Role
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sheriff)  // 보안관 (MaxHealth +1, 공개)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Deputy)   // 부관
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Outlaw)   // 무법자
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Renegade) // 배신자
	}

	// ---------------------------------------------------------------------------------
	//  Character.Job.* — 판타지 직업 12종. 스탯과 확률 보정의 정체성을 정한다 (GDD §7).
	//
	//  Role.* 과는 축이 완전히 다르다. 섞지 말 것:
	//    Role.*         = 진영(보안관/부관/무법자/배신자). 승리 조건과 은닉 정보. 숨김.
	//    Character.Job.* = 직업(전사/마법사/...). 체력·카드 사용 한도·판정 확률 보정. 공개.
	//  한 플레이어는 둘 다 하나씩 갖는다 — "무법자 마법사" 처럼.
	//
	//  ⚠️ 뱅! 원작 캐릭터 16종(PaulRegret 등)은 사거리/수트/응답 규칙의 예외였다.
	//     GDD §3 에서 그 규칙들이 전부 비활성화되어 기반이 사라졌으므로 직업으로 대체한다.
	//     구현 훅은 "규칙의 예외" 가 아니라 UBGDiceComponent::ApplyModifiers 가 읽는
	//     AttributeSet 의 WeightBonus*/WeightMult* 보정이다 (GDD §4.2).
	// ---------------------------------------------------------------------------------
	namespace Character
	{
		namespace Job
		{
			//	⚠️ 2026-08-14 전면 개명 (Docs/Data/Character-List.csv 기준).
			//	   태그와 실제 직업 이름이 어긋나 있던 것을 맞췄다. **부분 개명이 불가능하다** —
			//	   Mage 가 다른 직업으로 옮겨가 같은 문자열이 다른 뜻이 되기 때문이다.
			//	     Warrior→Knight   Mage→Mercenary   Rogue→Hunter     Cleric→Scout
			//	     Ranger→Priest    Paladin→Healer   Berserker→Mage   Alchemist→Diviner
			//	   Row 이름이 곧 태그 문자열이므로 DT_BGCharacterRow 도 함께 갈아엎어야 한다
			//	   (Docs/Data/DT_BGCharacterRow.json 임포트). 한쪽만 하면 전원이 무직이 된다.
			//
			//	   대괄호는 현재 구현 상태다. 태그와 Row 는 12종 모두 있지만, 능력이 실제로
			//	   동작하는 것은 그 표시가 없는 것들뿐이다.

			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Knight)     // 기사     — 체력 ↑. 기복 없는 국밥
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mercenary)  // 용병     — 공격 카드는 실패하지 않는다
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hunter)     // 사냥꾼   — 카드 사용 한도 +1
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Scout)      // 수색꾼   — 턴 시작 드로우 +1
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Priest)     // 사제     — 회복량 +1              [미구현]
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Healer)     // 치유사   — 턴 시작 회복, 체력 ↓   [체력만]
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mage)       // 마법사   — 공격 시 확률로 피해 +1 [미구현]
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Diviner)    // 점술가   — 대성공·대실패 승산 ×2
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Monk)       // 수도사   — 피해 반사, 체력 ↓      [체력만]
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Brawler)    // 무투가   — 공격 카드 사용한도 확률 유지
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Thief)      // 도둑     — 피해 시 카드 강탈      [미구현]
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Assassin)   // 암살자   — 대성공 시 1회성 카드   [미구현]
		}
	}

	// ---------------------------------------------------------------------------------
	//  SetByCaller.* — GE 수치를 런타임에 주입할 때 쓰는 태그 (피해/회복 크기).
	// ---------------------------------------------------------------------------------
	namespace SetByCaller
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage) // GE_BGDamage 크기
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal)   // GE_BGHeal 크기
	}
}
