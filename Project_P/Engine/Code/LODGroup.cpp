#include "epch.h"
#include "LODGroup.h"

CLODGroup::CLODGroup()
	: m_vRenderList({})
{
}

CLODGroup::~CLODGroup()
{
}

CLODGroup* CLODGroup::Create()
{
	return new CLODGroup();
}

CComponent* CLODGroup::Clone() const
{
	CLODGroup* clone = new CLODGroup();

	return clone;
}

HRESULT CLODGroup::Initialize()
{
	return S_OK;
}

void CLODGroup::Awake()
{
	InitializeMeshRenders();
}

void CLODGroup::OnPreCull()
{
}

void CLODGroup::OnPreRender()
{
}

void CLODGroup::Render_Editor()
{
}

void CLODGroup::Render()
{
}

void CLODGroup::OnPostRender()
{
}

void CLODGroup::OnDestroy()
{
}

void CLODGroup::InitializeMeshRenders()
{
	vector<CMeshRenderer*> meshList = GetTransform()->FindComponentsParentRecursive<CMeshRenderer>();

	for (auto mesh : meshList)
	{
		if (mesh->Get_GameObject()->IsActive() && mesh->Get_Enable())
			m_vRenderList.push_back(mesh);
	}
}
