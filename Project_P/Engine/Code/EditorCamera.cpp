#include "epch.h"
#include "EditorCamera.h"

CEditorCamera::CEditorCamera()
	: CCamera{}
	, m_sOptions({})
	, m_bRMouseDowned(false)
	, m_v2MouseDragDelta({})
	, m_v2PrevMosuePos({})
	, m_fPitch(0.f)
	, m_fYaw(0.f)
	, m_bMoving(false)
	, m_fMoveTimeTotal(0.25f)
	, m_fMoveTimeCur(0.f)
	, m_v3MoveFrom({})
	, m_v3MoveTo({})
	, m_bZoomDistToggle(false)
{
	m_strName = L"Editor Camera";
}

CEditorCamera::~CEditorCamera()
{
}

CEditorCamera* CEditorCamera::Create()
{
	return new CEditorCamera();
}

CComponent* CEditorCamera::Clone() const
{
	CDebug::LogError(L"This component cannot be cloned." + m_strName);
	return nullptr;
}

HRESULT CEditorCamera::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	m_fNear = 0.1f;
	m_fFar = 1000000.f;

	return S_OK;
}

void CEditorCamera::Update_Editor()
{
	const _bool isShift = CInput::GetInstance().GetKey_Editor(SHIFT);
	const _bool isCtrl = CInput::GetInstance().GetKey_Editor(CONTROL);

	CTransform& camTransform = *Get_Transform();

	if (m_bMoving)
	{
		m_fMoveTimeCur += DELTA_TIME;
		float t = m_fMoveTimeCur / m_fMoveTimeTotal;
		if (t > 1.f)
		{
			t = 1.f;
			m_bMoving = false;
		}

		_vector moveFrom = m_v3MoveFrom.toXMVector();
		_vector moveTo = m_v3MoveTo.toXMVector();
		float smoothT = t * t * (3.f - 2.f * t);

		_vector newPos = XMVectorLerp(moveFrom, moveTo, smoothT);
		Get_Transform()->Add_Position(vector3(newPos));
	}
	else
	{
		if (isShift)
			m_sOptions.crtMoveSpeed = m_sOptions.moveSpeed * 2.5f;
		else
			m_sOptions.crtMoveSpeed = m_sOptions.moveSpeed;

		const _bool isRightMouseDown = CInput::GetInstance().GetMouseButton_Editor(1);
		const _bool isMiddleMouseDown = CInput::GetInstance().GetMouseButton_Editor(2);

		if (isRightMouseDown || isMiddleMouseDown)
		{
			vector2Int currentMouse = CInput::GetInstance().GetMousePos_Editor();

			if (!m_bRMouseDowned)
			{
				m_v2PrevMosuePos = CInput::GetInstance().GetMousePos_Editor();
				m_bRMouseDowned = true;
				return;
			}

			_float horizontal = CInput::GetInstance().GetAxis_Editor(L"Horizontal");
			_float vertical = CInput::GetInstance().GetAxis_Editor(L"Vertical");

			if (!isCtrl)
			{
				camTransform.Add_Position(camTransform.Get_Directions().right * horizontal * DELTA_TIME * m_sOptions.crtMoveSpeed);
				camTransform.Add_Position(camTransform.Get_Directions().forward * vertical * DELTA_TIME * m_sOptions.crtMoveSpeed);

				if (CInput::GetInstance().GetKey_Editor(Q))
					camTransform.Add_Position(camTransform.Get_Directions().down * DELTA_TIME * m_sOptions.crtMoveSpeed);
				if (CInput::GetInstance().GetKey_Editor(E))
					camTransform.Add_Position(camTransform.Get_Directions().up * DELTA_TIME * m_sOptions.crtMoveSpeed);

				m_v2MouseDragDelta = (currentMouse - m_v2PrevMosuePos).to_vector2();
				m_v2PrevMosuePos = currentMouse;

				if (isRightMouseDown)
				{
					if (m_v2MouseDragDelta != vector2::zero())
					{
						m_fYaw = m_v2MouseDragDelta.x * DELTA_TIME * m_sOptions.rotateSpeed;
						m_fPitch = m_v2MouseDragDelta.y * DELTA_TIME * m_sOptions.rotateSpeed;

						camTransform.Add_EulerAngles(m_fPitch, m_fYaw, 0.f);
					}
				}
			}

			if (isShift)
				m_sOptions.crtDragSpeed = m_sOptions.dragSpeed * 2.f;
			else
				m_sOptions.crtDragSpeed = m_sOptions.dragSpeed;

			if (isMiddleMouseDown)
			{
				camTransform.Add_Position(camTransform.Get_Directions().left * m_v2MouseDragDelta.x * DELTA_TIME * m_sOptions.crtDragSpeed);
				camTransform.Add_Position(camTransform.Get_Directions().up * m_v2MouseDragDelta.y * DELTA_TIME * m_sOptions.crtDragSpeed);
			}
		}
		else
		{
			m_bRMouseDowned = false;
			m_v2MouseDragDelta = vector2::zero();
		}
	}

	const _float _wheel = CInput::GetInstance().GetAxis_Editor(L"Mouse ScrollWheel");

	if (_wheel != 0)
	{
		if (m_eCamViewMode == ViewMode::PERSPECTIVE)
		{
			if (!isShift)
				camTransform.Add_Position(camTransform.Get_Directions().forward * _wheel * DELTA_TIME * m_sOptions.zoomSpeed);
		}
		else
		{
			m_fSize -= _wheel * DELTA_TIME * m_sOptions.zoomSpeed;
		}
	}
}

void CEditorCamera::Update()
{
	m_fAspect = static_cast<_float>(CEditor::GetInstance().Get_ScreenResolution().x) / (CEditor::GetInstance().Get_ScreenResolution().y);

	Bind_ViewMatrix();
	Bind_ProjectionMatrix();
}

void CEditorCamera::OnDestroy()
{
}
