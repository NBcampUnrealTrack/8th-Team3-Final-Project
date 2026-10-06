# 캐릭터별 무기 디자인 멀티뷰 시트

2026-10-01 · 내장 ImageGen으로 제작 · A포즈 원본 14종 참고

직업 캐릭터 12종 + 몬스터 2종. 각 PNG는 1536×1024이며, 캐릭터의 로우폴리 스타일·의상 색상·문양을 무기에 반영했다. 설계상 기본은 정면·측면·후면·사선 뷰다. 석궁은 상면·측면·정면·사선, 활은 프로파일·정면·후면·사선, 건틀릿은 손등·측면·손바닥·사선과 좌우 한 쌍을 포함한다.

## 목록과 권장 크기

H는 해당 캐릭터의 발바닥부터 정수리까지 높이. 아래 값은 디자인 제안이며 실제 모델 치수 측정값이 아니다. 장착 시 캐릭터 손 크기와 그립을 우선해 조절한다. 시트 안의 서로 다른 소품/행은 같은 축척이 아닐 수 있다.

| 캐릭터 / 무기 | 권장 크기 | 파일 |
|---|---|---|
| 기사 — 검·방패 | 검 0.50H / 방패 0.32H | [PNG](01_male_knight_sword_shield_multiview_v1.png) |
| 여성 용병 — 전투 도끼 | 0.65H | [PNG](02_female_mercenary_battleaxe_multiview_v1.png) |
| 남성 헌터 — 석궁 | 0.42H | [PNG](03_male_hunter_crossbow_multiview_v1.png) |
| 여성 스카우트 — 활·화살 | 활 0.70H | [PNG](04_female_scout_recurve_bow_multiview_v1.png) |
| 프리스트 — 태양 메이스 | 0.40H | [PNG](05_male_priest_sun_mace_multiview_v1.png) |
| 힐러 — 잎 지팡이 | 0.88H | [PNG](06_female_healer_leaf_staff_multiview_v1.png) |
| 위저드 — 수정 지팡이 | 0.95H | [PNG](07_male_wizard_crystal_staff_multiview_v1.png) |
| 아르카나 — 마법 카드·케이스 | 카드 0.11H / 케이스 0.13H | [PNG](08_female_arcana_card_focus_multiview_v1.png) |
| 몽크 — 봉 | 1.03H | [PNG](09_male_monk_quarterstaff_multiview_v1.png) |
| 여성 파이터 — 건틀릿 | 손목~너클 0.14H | [PNG](10_female_fighter_gauntlets_multiview_v1.png) |
| 도적 — 짧은 세이버 | 0.38H | [PNG](11_male_thief_short_sword_multiview_v1.png) |
| 암살자 — 역수용 단검 | 0.22H | [PNG](12_female_assassin_reverse_dagger_multiview_v1.png) |
| 스켈레톤 — 녹슨 검 | 0.43H | [PNG](13_skeleton_rusted_sword_multiview_v1.png) |
| 임프 — 삼지창 | 0.80H | [PNG](14_demon_imp_pitchfork_multiview_v1.png) |

## 3D 제작 적용

- 먼저 정면 또는 주 실루엣 뷰로 형태를 잡고, 측면으로 두께를 정한다. 사선 뷰는 볼륨 참고용이다. 생성된 컨셉 시트이므로 뷰 사이의 미세한 장식·두께·투영 차이는 모델링 단계에서 하나의 형태로 통일한다.
- 석궁의 활 곡률과 시위 위치는 TOP을 기준으로 한다. 시위는 양 끝에 연결된 별도 오브젝트로 만들고, 볼트 홈 위를 지나도록 높이를 확보한다. 발사 애니메이션에서 변형할 부분은 분리한다.
- 활 몸통·활시위·화살은 분리한다. 기사 검·방패와 아르카나 카드·케이스도 각각 분리한다.
- 손에 쥐는 무기의 피벗은 손잡이 중심에 둔다. 암살자 단검은 오른손 역수 장착을 기준으로 회전을 맞춘다.
- 건틀릿은 한쪽을 기준으로 만들고 반대편을 미러링한 뒤 실제 손 메시와 간섭을 확인한다. 시트는 개별 손가락 리깅을 규정하지 않는다.
- 금속·목재·가죽·천·보석 경계를 큰 면으로 유지하고, 작은 리벳과 문양은 텍스처/얕은 양각으로 단순화할 수 있다.

## 제작 기록

원본 이미지 경로와 실제 프롬프트(수정 단계 포함)는 prompts.json에 기록했다. gallery.html을 브라우저로 열면 모든 시트를 한 번에 볼 수 있다. 최종 선택본만 이 폴더에 포함했다. 생성 캐시 원본은 별도로 보존했다.


## 프리스트 추가 무기: 펼친 성서

[멀티뷰 PNG](05b_male_priest_open_scripture_multiview_v1.png) · [프롬프트 및 모델링 안내](05b_priest_open_scripture_prompt.txt)

페이지 면, 바깥 표지, 아래쪽 단면, 사선 뷰를 포함한다. 기존 14종에 추가한 시트로 총 15장이다.
