# Unreal MCP 가이드

이 프로젝트는 Epic 공식 **ModelContextProtocol** 플러그인을 켜서, 에디터를 켜 두면
Claude Code(또는 다른 MCP 클라이언트)가 실행 중인 에디터 상태를 직접 읽고 조작할 수 있다.
AI가 "코드만 보고 추측" 하는 대신 실제 블루프린트 그래프, DT 값, PIE 중 런타임 상태를
직접 열어볼 수 있다는 뜻이다.

## 이번에 적용한 설정

| 파일 | 내용 | 커밋됨 |
|---|---|---|
| `BG.uproject` | `ModelContextProtocol`, `MCPClientToolset` 는 이미 켜져 있었으나 **`AllToolsets` 가 빠져 있었다.** `ModelContextProtocol` 은 서버·전송만 담당하고 실제 30여 종 툴(액터/블루프린트/머티리얼/…)은 `AllToolsets` 가 제공한다. 이게 없으면 서버는 뜨는데 툴이 하나도 안 보인다. 이번에 추가함. | ✅ |
| `.mcp.json` (프로젝트 루트) | `unreal-mcp` → `http://127.0.0.1:8000/mcp`. 이미 올바르게 설정돼 있었음. | ✅ |
| `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` | `[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]` 섹션에 `bAutoStartServer=True`, `ServerPortNumber=8000`, `ServerUrlPath=/mcp` 추가. 에디터를 켜면 서버가 자동으로 뜬다. | ❌ (개인 설정, `Saved/` 는 `.gitignore` 대상) |

**주의**: 마지막 ini 설정은 로컬 전용이라 각자 에디터에서 한 번씩 추가해야 한다. 안 해 두면
매번 에디터 콘솔(`~`)에서 `ModelContextProtocol.StartServer` 를 직접 실행해야 서버가 뜬다.

## 새로 가능해진 것

`list_toolsets` 로 확인한 실제 등록 목록 기준. 이 프로젝트와 관련 있는 것 위주로 정리:

- **에디터 상태 조회/조작** — `EditorAppToolset`(콘솔 변수, 액터·에셋 선택, 뷰포트 카메라, PIE 시작/정지), `LogsToolset`(로그 카테고리 verbosity 제어)
- **액터/블루프린트/에셋** — `ActorTools`, `BlueprintTools`, `AssetTools`, `ObjectTools`(프로퍼티 조회·설정), `DataTableTools`/`DataAssetTools`/`CurveTableTools`, `MaterialTools`/`MaterialInstanceTools`, `TextureTools`, `StringTableTools`
  → `DT_BGCard` 의 `OutcomeEffects`, `AbilityByCardId` TMap 같은 설정값을 에디터를 직접 열지 않고 조회/수정할 수 있다.
- **GAS 전용** — `AttributeSetToolset`, `GameplayCueToolset`, `AbilitySystemInspectorToolset`
  → PIE 중 `AbilitySystemComponent` 의 런타임 상태(어트리뷰트 값, 걸려 있는 GameplayEffect)를 직접 조회 가능. 사망/연출/카메라 파이프라인처럼 "코드는 맞는데 실제로 재생되는지" 확인이 필요한 작업에 바로 쓸 수 있다.
- **GameplayTagsToolset** — 프로젝트 `GameplayTagsManager` 의 태그 조회/관리.
  ⚠️ `Card.Id.Bang` 류는 게임 데이터다. MCP로 태그를 리네임/치환할 때도 CLAUDE.md의 `Bang`/`BG` 트랩 규칙이 그대로 적용된다 — 일괄 치환 금지.
- **애니메이션/연출** — `SequencerTools` 계열(시퀀스 생성/바인딩/키프레임/Control Rig), `NiagaraToolsets`(이미터/시스템 생성·수정, 런타임 컴포넌트 조작)
  → 시네마틱 카메라, 사망 연출 몽타주·AnimNotify 배치 여부를 실제로 열어서 확인·수정할 수 있다(이전엔 코드/에셋 존재 여부만 grep 으로 추정했던 부분).
- **PvE/AI** — `BehaviorTreeTools` (BT 에셋 검사)
- **UI** — `UMGToolSet` (WBP 위젯 트리 조회·수정), `SlateInspectorToolset` (에디터 UI 스크린샷/자동 입력)
- **메시/피직스** — `SkeletalMeshTools`, `StaticMeshTools`, `PhysicsAssetToolset`
- **테스트/자동화** — `AutomationTestToolset` (자동화 테스트 검색/실행/결과 조회)
- **기타** — `ConfigSettingsToolset`(`DefaultGame.ini` 등 설정 조회/수정), `PluginToolset`, `GameFeaturesToolset`, `AgentSkillToolset`

## 사용법

툴 서치 모드(기본값)라 `tools/list` 에는 메타 툴 3개만 보인다. 실제 툴은 이 세 개를 거쳐서 쓴다.

1. `list_toolsets` — 등록된 전체 툴셋 이름과 설명을 본다.
2. `describe_toolset(toolset_name=...)` — 쓰려는 툴셋의 개별 툴 이름·입력 스키마를 확인한다.
3. `call_tool(toolset_name=..., tool_name=..., arguments={...})` — 실제로 실행한다.

예시 — 현재 게임플레이 태그 트리 조회:
```
call_tool(toolset_name="GameplayTagsToolset.GameplayTagsToolset",
          tool_name="<describe_toolset 로 확인한 이름>",
          arguments={...})
```

## 안전 규칙

- **저장은 전/후 모두.** 여러 에셋에 걸치는 변경 전엔 저장, 끝나고도 저장. MCP 편집은 되돌리기가 항상 되는 게 아니다.
- **컴파일 중엔 기다린다.** C++/셰이더 컴파일이 돌고 있으면 호출이 멈추거나 이상하게 실패한다. C++ 재빌드는 `LiveCodingToolset.CompileLiveCoding` 로 하고 끝날 때까지 기다린다.
- **결과를 항상 확인한다.** 블루프린트 컴파일/머티리얼 편집 등은 예외 없이 조용히 실패를 돌려줄 수 있다. 성공이라고 명시되지 않으면 실패로 간주한다.
- **PIE 상태를 신경 쓴다.** 에셋 생성류 툴은 PIE 중엔 동작이 달라진다. 결과가 이상하면 PIE가 돌고 있는지부터 확인.

## 트러블슈팅

| 증상 | 원인/대응 |
|---|---|
| Claude Code에서 `unreal-mcp` 가 `ECONNREFUSED` | 에디터가 안 떠 있거나(가장 흔함), 떠 있어도 서버가 자동시작 안 됨. 에디터를 켜거나 콘솔에서 `ModelContextProtocol.StartServer` 실행. |
| 서버는 떴는데 `list_toolsets` 가 비어 있음 | `.uproject` 에 `AllToolsets` 가 빠짐. |
| 특정 툴셋(예: Niagara)이 안 보임 | 콘솔에서 `ModelContextProtocol.RefreshTools` 실행. 여전히 없으면 해당 툴셋 전용 플러그인이 꺼져 있는지 확인. |
| 포트 충돌("Failed to listen on port") | 개인 ini의 `ServerPortNumber` 를 바꾸고 `ModelContextProtocol.GenerateClientConfig ClaudeCode` 로 `.mcp.json` 갱신. |
| 포트는 열렸는데 Claude Code 세션에서 여전히 연결 실패로 보임 | 세션이 시작 시점에 이미 연결을 실패해 캐시된 상태일 수 있다. `/mcp` 로 재연결 시도하거나 세션을 재시작한다. |
