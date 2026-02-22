# Rigidbody Translate를 SetVelocity 기반으로 바꾸기

가능합니다. 보통 물리 기반 이동에서는 `Transform.Translate`보다 `Rigidbody.velocity`(또는 `linearVelocity`)를 사용하는 편이 충돌/마찰/관성 처리에 더 자연스럽습니다.

## 기본 전환 방식

```csharp
using UnityEngine;

public class PlayerMove : MonoBehaviour
{
    [SerializeField] private Rigidbody rb;
    [SerializeField] private float speed = 5f;

    private void FixedUpdate()
    {
        float h = Input.GetAxisRaw("Horizontal");
        float v = Input.GetAxisRaw("Vertical");

        Vector3 input = new Vector3(h, 0f, v).normalized;
        Vector3 vel = rb.velocity;
        vel.x = input.x * speed;
        vel.z = input.z * speed;
        rb.velocity = vel;
    }
}
```

## 핵심 포인트

- 입력/물리 갱신은 `FixedUpdate`에서 처리합니다.
- 중력 점프를 유지하려면 `y`축 속도는 덮어쓰지 않고 기존 값을 보존합니다.
- `Translate`와 `velocity`를 같은 오브젝트에서 섞어 쓰면 충돌이 어색해질 수 있어 한 방식으로 통일하는 것이 좋습니다.
- 급정지가 필요하면 입력이 0일 때 `x/z`를 0으로 두고, 관성을 원하면 감속 로직을 별도로 넣습니다.

## 언제 Translate가 더 나은가

- 물리 영향을 받지 않는 UI 연출 오브젝트
- 충돌 정밀도보다 단순 이동이 중요한 경우

플레이어/적 캐릭터처럼 물리 반응이 중요한 객체라면 `SetVelocity` 기반이 일반적으로 더 적합합니다.
