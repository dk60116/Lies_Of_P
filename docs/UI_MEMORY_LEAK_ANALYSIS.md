# 게임 종료 시 UI 메모리 릭 원인 분석

## 결론 요약
- UI 계열 릭의 핵심 원인은 `CCanvas`의 참조 카운트 소유권 불일치다.
- `CScene::Add_Canvas`에서 `AddRef()`를 호출하지만, 대응되는 `Release()`가 `Remove_Canvas` 및 `SceneRelease` 경로에 없다.
- 그 결과 종료 시 `CGameObject`가 컴포넌트를 해제해도 `CCanvas`의 참조 카운트가 0이 되지 않아 소멸되지 않는다.

## 1) 종료 시점 소유권 흐름

### 1-1. Canvas 등록 시 추가 참조 획득
- `CGameObject::AddComponent<CCanvas>()` 경로에서 `CScene::Add_Canvas`가 호출된다.
- `CScene::Add_Canvas`는 리스트에 넣은 뒤 `AddRef()`를 수행한다.

따라서 `CCanvas`는 최소 2개 소유권을 갖는다.
- 컴포넌트 리스트(`CGameObject::m_lComponentList`) 소유권
- 씬 Canvas 리스트(`CScene::m_lCanvasList`) 소유권

### 1-2. 종료 시 해제 경로
- `CScene::SceneRelease()`는 먼저 `m_lCanvasList.clear()`만 수행한다.
- 리스트 원소들에 대해 `Release()`를 호출하지 않는다.
- 이후 `m_lObjectList`의 오브젝트를 `Safe_Release`로 해제한다.
- `CGameObject::OnDestroy()`에서 컴포넌트별로 `OnDestroy()` 후 `Safe_Release(*it)`를 호출한다.

문제는 이 시점 `CCanvas`가 여전히 씬 리스트에서 획득했던 참조를 내부 카운트로 유지한다는 점이다.
리스트에서 포인터를 지워도 참조 카운트는 감소하지 않기 때문에, 컴포넌트 해제 1회만으로는 0이 되지 않는다.

## 2) UI 전용으로 보이는 이유
- `CScene::Add_Canvas`만 유일하게 명시적 `AddRef()`를 수행한다.
- 대응되는 `Remove_Canvas`는 단순 `remove`만 수행한다.
- Camera/Light 리스트와 달리 Canvas만 별도 참조 증가를 하기 때문에, 동일 종료 시퀀스에서도 UI 계열에서만 잔존 객체가 관측된다.

## 3) 2차 누수 포인트

### 3-1. `CRectTransform::m_pParentRect` 해제 누락
- `CRectTransform::SetParent`에서 `m_pParentRect->AddRef()`를 수행한다.
- 그러나 `CRectTransform::OnDestroy`에서는 `m_pUI`만 해제하고 `m_pParentRect`를 해제하지 않는다.

UI 계층이 깊을수록 `m_pParentRect` 추가 참조가 남아 종료 시 누수 규모가 증가할 수 있다.

### 3-2. 종료 루틴의 제한적 정리
- `CMainProcess::Release_MainApp()`는 현재 `CDebug::Release()`만 호출한다.
- 엔진 매니저들의 명시적 종료 순서를 거치지 않기 때문에, 디버그 누수 체크 시점과 정적 객체 소멸 시점이 엇갈리면 누수가 더 크게 보고될 수 있다.

## 4) 재현 시 관찰 포인트
- 디버그 모드에서 `_CRTDBG_LEAK_CHECK_DF`가 활성화되어 종료 시 누수를 덤프한다.
- HUD를 포함한 씬을 로드 후 종료하면 UI 컴포넌트 계열 주소가 반복적으로 남는다.
- 특히 Canvas 수와 비례하는 잔존이 나타나면 `Add_Canvas`/`Remove_Canvas` 불균형 가능성이 매우 높다.

## 5) 정리
UI 종료 릭은 단일 버그가 아니라, 다음 2개가 결합되어 증폭된다.
- 1차 원인: `CCanvas`의 참조 카운트 증가(`Add_Canvas`)와 감소(`Remove_Canvas`/`SceneRelease`)의 불일치
- 2차 원인: `CRectTransform`의 `m_pParentRect` 참조 해제 누락

두 지점을 동시에 정리해야 종료 시 UI 관련 잔존이 안정적으로 사라진다.
