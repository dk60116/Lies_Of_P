# Eve_AnimatorController / CPlayerState_Attack 분석

## 1) 핵심 파라미터 매핑
- 애니메이터는 공격 관련 파라미터로 `Attack`(Trigger), `attackCombo`(int), `comboContinue`(bool), `isAttack`(bool), `isStrongAttack`(bool)을 사용한다.
- 코드에서는 동일 의도를 `SetTrigger("Attack")`, `SetInt("AttackCombo")`, `SetBool("comboContinue")`, `SetBool("isAttack")`, `SetBool("isStrongAttack")`로 제어한다.
- 문자열 키에서 `attackCombo`(소문자 a)와 `AttackCombo`(대문자 A)가 혼재되어 있어, 파라미터명이 대소문자 구분되는 구현이라면 연결 불일치 가능성이 있다.

## 2) 진입/분기 구조
- Any State에서 기본 약공 진입은 `Attack && attackCombo==0 && isAttack==true`로 `Attack_Light_01`로 들어간다.
- Any State에서 강공 진입은 `isStrongAttack==true && attackCombo==0 && Attack`로 `Attack_Strong_01`로 들어간다.
- `Enter()`는 `Attack` 트리거를 발사하고 `isAttack=true`, `comboContinue=false`, `AttackCombo=0`으로 초기화한다.

## 3) 라이트 콤보 체인
- `Attack_Light_01 -> 02 -> 03 -> 04`는 공통적으로 `comboContinue==true` 조건으로 이어진다.
- 각 라이트 클립에 대해 코드가 `_Enter`, `_Term`, `_Limit`, `_Exit` 트리거를 등록한다.
- `_Enter`: `m_iCrtCombo` 증가, `comboContinue=false` 리셋, 체인 가능 플래그 활성화.
- `_Term`: `m_bUnderTerm=false`로 바꾸고, 입력 예약(`m_bPressedContinue`)이 있으면 즉시 `ContinueCombo()`.
- `_Limit`: `m_bCanContinue=false`, `m_bUnderLimit=false`로 체인 종료.
- `_Exit`: 상태 `Exit()` 호출.

## 4) 혼합 체인(LS/SS/SL)
- 애니메이터 전이:
  - `Attack_Light_01 -> Eve_Attack_LS12` (`comboContinue && isStrongAttack`)
  - `Eve_Attack_LS12 -> Eve_Attack_SS23` (`isStrongAttack && comboContinue`)
  - `Eve_Attack_LS12 -> Eve_Attack_SL23` (`isAttack && comboContinue`)
  - `Eve_Attack_SS23 -> Eve_Attack_SS34` (`isStrongAttack && comboContinue`)
- 코드도 동일하게 LS12/SS23/SS34/SL23 각각 Start/Term/Limit/End 트리거를 별도 등록해 체인 타이밍을 관리한다.

## 5) 입력 처리 타이밍 모델
- `Update()`에서 약공 입력 시 `m_bStrong=false`, 강공 입력 시 `m_bStrong=true`로 모드를 즉시 전환한다.
- 매 프레임 `isAttack=!m_bStrong`, `isStrongAttack=m_bStrong`를 애니메이터에 반영한다.
- 공격 입력 발생 시:
  - 아직 체인 윈도우 내부(`m_bCanContinue && m_bUnderTerm`)면 `m_bPressedContinue=true`로 예약.
  - 아니면 즉시 `ContinueCombo()` 시도.
- 즉, 코드 의도는 "윈도우 내 선입력 버퍼 + 윈도우 종료 시 즉시 소비" 구조다.

## 6) 상태 종료 조건
- 일반적으로 각 클립 End 트리거에서 `Exit()`.
- 추가로 `!m_bUnderLimit` 상태에서 이동 홀드 입력 또는 큰 턴이면 `Exit()`.
- `Exit()`는 `comboContinue/isAttack/isStrongAttack`를 false로 내리고 이동/회전/점프를 복구한다.

## 7) 잠재 이슈 포인트
- 파라미터명 대소문자 혼재:
  - 컨트롤러: `attackCombo`
  - 코드: `AttackCombo`
  - 런타임이 대소문자 구분이면 콤보 인덱스 전이 조건이 동작하지 않을 수 있다.
- 디버그 로그 잔존:
  - `Eve_Attack_SL23_Start`에서 `CDebug::LogError("Enter")`.
  - `Update()` 말미에 `CDebug::LogError(m_bPressedContinue)`.
- `Step` 구조체는 선언되어 있으나 실제 사용되지 않는다.
- `m_bLastContinue` 관련 분기(무한 루프형 라이트 재시작)는 핵심 로직이 주석 처리되어 현재 실질적으로 비활성 상태다.
