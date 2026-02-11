#include "epch.h"
#include "InspectorBox.h"
#include "Resources.h"
#include "Transform.h"
#include "RectTransform.h"
#include "Camera.h"
#include "Light.h"
#include "MeshRenderer.h"
#include "MeshFilter.h"
#include "SkinnedMeshRenderer.h"
#include "Animator.h"
#include "UI.h"
#include "Canvas.h"
#include "Terrain.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

CInspectorBox::CInspectorBox()
	: m_fRXDrag(0.f)
    , m_fRYDrag(0.f)
    , m_fRZDrag(0.f)
    , m_pPreviewTexture(nullptr)
    , m_previewAssetPath()
{
}

CInspectorBox::~CInspectorBox()
{
	OnDestroy();
}

CInspectorBox* CInspectorBox::Create()
{
	CInspectorBox* newBox = new CInspectorBox();

	if (FAILED(newBox->Initialize()))
	{
		delete(newBox);
		newBox = nullptr;
		return nullptr;
	}

	newBox->m_strBoxName = L"Inspector";

	return newBox;
}

void CInspectorBox::Render()
{
	CEditor& editor = CEditor::GetInstance();
	const CEditor::EDITORWINOPTION& editorOption = editor.Get_Options();
	CScene* currentScene = CSceneManager::GetInstance().Get_CrtScene();

	_float width = static_cast<_float>(editorOption.inspectorWidth);

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	ImVec2 panelSize = ImVec2(width, viewport->Size.y - editorOption.topBarHeight);

	ImGui::SetNextWindowPos
	(
		ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + editorOption.topBarHeight),
		0,
		ImVec2(1.0f, 0.0f)
	);

	ImGui::SetNextWindowSize(panelSize);

	ImGui::Begin
    (
        CEngineString::WStringToString(m_strBoxName).c_str(),
		nullptr,
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_HorizontalScrollbar
    );

    CGameObject* selectedObj = editor.Get_SelectedGameObject();

    if (selectedObj)
    {
        _bool active = selectedObj->IsActive();

        _float baseY = ImGui::GetCursorPosY();

        Toggle_Begin();

        if (ImGui::Checkbox("##ActiveToggle", &active))
            selectedObj->SetActive(active);

        CGameObject* s_NameTarget = nullptr;
        string  s_EditName;
        if (s_NameTarget != selectedObj)
        {
            s_NameTarget = selectedObj;
            s_EditName = CEngineString::WStringToString(selectedObj->Get_ObjectName());
        }

        ImGui::SameLine(0.0f, 6.0f);

        ImGui::SetCursorPosY(baseY);
        ImGui::Text(("[" + to_string(selectedObj->Get_UniqueID()) + "] ").c_str());
        ImGui::SameLine();
        ImGui::SetCursorPosY(baseY + 3.f);
        ImGui::SetNextItemWidth(140.0f);
        string label = "##ObjName" + to_string(selectedObj->Get_UniqueID());
        if (ImGui::InputText(label.c_str(), &s_EditName,
            ImGuiInputTextFlags_AutoSelectAll |
            ImGuiInputTextFlags_EnterReturnsTrue))
        {
            wstring targetName = CEngineString::StringToWString(s_EditName);
            selectedObj->Set_ObjectName(targetName);
        }

        Toggle_End();

        if (!selectedObj)
            return;

        ImGui::Text(selectedObj->IsBoneTransform() ? "Bone" : "");

        if (!selectedObj->GetComponent<CRectTransform>())
            ShowTransform(selectedObj);
        else
            ShowRectTransform(selectedObj);

        ShowComponents(selectedObj);
    }
    else
        ImGui::Text("No object selected.");

    if (!selectedObj)
    {
        RenderSelectedAssetInfo(editor.Get_SelectedAssetPath());
        RenderSelectedAssetPreview(editor.Get_SelectedAssetPath());
    }

	ImGui::End();
}

void CInspectorBox::OnDestroy()
{
}

void CInspectorBox::ShowTransform(CGameObject* _obj)
{
    CTransform* transform = _obj->Get_Transform();

    if (transform)
    {
        ImGui::Text("Transform");

        const _float LabelWidth = 60.f;
        const _float boxWidth = 34.f;

        // Position
        _float3 position = transform->Get_LocalPosition();
        if (ImGui::BeginTable("Position Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            ImGui::TableSetColumnIndex(1);
            
            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &position.x, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &position.y, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &position.z, 0.f, 0.f))
                transform->Set_LocalPosition(position);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        // Rotation
        _float3 rotation = transform->Get_LocalEulerAngles();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Rotation");
            ImGui::TableSetColumnIndex(1);

            const _float rotationDeg = 2.5f;

            // X
            _float prevX = m_fRXDrag;
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &rotation.x, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            _float prevY = m_fRYDrag;
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Z
            _float prevZ = m_fRZDrag;
            ImGui::TextUnformatted("Z");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
            {
                transform->Set_LocalEulerAngles(rotation);
            }
            ImGui::PopItemWidth();

            prevX = 0.f;
            prevY = 0.f;
            prevZ = 0.f;

            ImGui::EndTable();
        }

        // Scale
        _float3 scale = transform->Get_LocalScale();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Scale");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &scale.x, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &scale.y, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Z
            ImGui::TextUnformatted("Z"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Z", &scale.z, 0.f))
                transform->Set_LocalScale(scale);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }
    }
}

void CInspectorBox::ShowRectTransform(CGameObject* _obj)
{
    CRectTransform* rectTransform = dynamic_cast<CRectTransform*>(_obj->Get_Transform());

    if (rectTransform)
    {
        ImGui::Text("Rect Transform");

        const _float LabelWidth = 0.f;
        const _float boxWidth = 34.f;

        vector2 position = rectTransform->Get_AnchoredPosition();
        if (ImGui::BeginTable("Base Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Pos X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos X", &position.x, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Pos Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Pos Y", &position.y, 0.f))
                rectTransform->Set_AnchoredPosition(position);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        _float width = rectTransform->Get_Width();
        _float height = rectTransform->Get_Height();
        if (ImGui::BeginTable("Rotation Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("Width"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Width", &width, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Height"); 
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Height", &height, 0.f))
                rectTransform->Set_WidthHeight(vector2(width, height));
            ImGui::PopItemWidth();

            ImGui::SameLine();

            ImGui::EndTable();
        }

        vector2 pivot = rectTransform->Get_Pivot();
        if (ImGui::BeginTable("Pivot Table", 2, ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
            ImGui::TableSetupColumn("Value");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Pivot");
            ImGui::TableSetColumnIndex(1);

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##X", &pivot.x, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::SameLine();

            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Y", &pivot.y, 0.f))
                rectTransform->Set_Pivot(pivot);
            ImGui::PopItemWidth();

            ImGui::EndTable();
        }

        CRectTransform::Anchors anchors = rectTransform->Get_Anchors();
        if (ImGui::TreeNode("Anchors"))
        {
            ImGui::Text("Min");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmin", &anchors.min.x, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();
            
            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymin", &anchors.min.y, 0.f))
                rectTransform->Set_AnchorsMin(anchors.min.x, anchors.min.y);
            ImGui::PopItemWidth();

            ImGui::Text("Max");
            ImGui::SameLine();

            // X
            ImGui::TextUnformatted("X");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Xmax", &anchors.max.x, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::SameLine();
            // Y
            ImGui::TextUnformatted("Y");
            ImGui::SameLine();
            ImGui::PushItemWidth(boxWidth);
            if (ImGui::InputFloat("##Ymax", &anchors.max.y, 0.f))
                rectTransform->Set_AnchorsMax(anchors.max.x, anchors.max.y);
            ImGui::PopItemWidth();

            ImGui::TreePop();
        }

        vector3 rotation = rectTransform->Get_LocalEulerAngles();
        ImGui::Text("Rotation");
        ImGui::SameLine();

        // X
        _float prevX = m_fRXDrag;
        ImGui::TextUnformatted("X");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##X", &rotation.x, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Y
        _float prevY = m_fRYDrag;
        ImGui::TextUnformatted("Y");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Y", &rotation.y, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        // Z
        _float prevZ = m_fRZDrag;
        ImGui::TextUnformatted("Z");
        ImGui::SameLine();
        ImGui::PushItemWidth(boxWidth);
        if (ImGui::InputFloat("##Z", &rotation.z, 0.f))
        {
            rectTransform->Set_LocalEulerAngles(rotation);
        }
        ImGui::PopItemWidth();

        prevX = 0.f;
        prevY = 0.f;
        prevZ = 0.f;
    }
}


void CInspectorBox::ShowComponents(CGameObject* _obj)
{
    if (!_obj)
        return;

    ImGui::Separator();
    ImGui::Text("Components");

    list<CComponent*>& components = _obj->Get_ComponentList();
    for (CComponent* component : components)
    {
        if (!component)
            continue;

        string componentName = CEngineString::WStringToString(component->Get_UName());
        if (componentName.empty())
            continue;

        const string headerLabel = componentName + "##" + to_string(reinterpret_cast<uintptr_t>(component));
        if (ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            if (CMeshRenderer* meshRenderer = dynamic_cast<CMeshRenderer*>(component))
                RenderMeshRendererComponent(meshRenderer);

            if (CMeshFilter* meshFilter = dynamic_cast<CMeshFilter*>(component))
                RenderMeshFilterComponent(_obj, meshFilter);
        }
    }

    ShowAddComponentMenu(_obj);
}

void CInspectorBox::RenderMeshRendererComponent(CMeshRenderer* _meshRenderer)
{
    if (!_meshRenderer)
        return;

    CMeshFilter* meshFilter = _meshRenderer->Get_MeshFilter();
    if (!meshFilter)
    {
        ImGui::TextUnformatted("MeshFilter: None");
        return;
    }

    ImGui::TextUnformatted("MeshFilter: Linked");

    CMeshBuffer* meshBuffer = meshFilter->Get_MeshBuffer();
    if (!meshBuffer)
    {
        ImGui::TextUnformatted("MeshBuffer: None");
        return;
    }

    string meshName = CEngineString::WStringToString(meshBuffer->Get_ResourceName());
    ImGui::Text("MeshBuffer: %s", meshName.c_str());
}

void CInspectorBox::RenderMeshFilterComponent(CGameObject* _obj, CMeshFilter* _meshFilter)
{
    if (!_obj || !_meshFilter)
        return;

    vector<pair<string, CMeshBuffer*>> meshOptions;
    CResources& resources = CResources::GetInstance();

    for (auto& entry : resources.m_mGameResourceList)
    {
        CMeshBuffer* meshBuffer = dynamic_cast<CMeshBuffer*>(entry.second);
        if (!meshBuffer)
            continue;

        string resourceName = CEngineString::WStringToString(meshBuffer->Get_ResourceName());
        meshOptions.push_back({ resourceName, meshBuffer });
    }

    sort(meshOptions.begin(), meshOptions.end(), [](const auto& a, const auto& b)
    {
        return a.first < b.first;
    });

    CMeshBuffer* currentMeshBuffer = _meshFilter->Get_MeshBuffer();
    string currentName = "None";
    _int currentIndex = -1;

    for (_uint i = 0; i < meshOptions.size(); ++i)
    {
        if (meshOptions[i].second == currentMeshBuffer)
        {
            currentIndex = static_cast<_int>(i);
            currentName = meshOptions[i].first;
            break;
        }
    }

    const string comboLabel = "MeshBuffer##" + to_string(_obj->Get_UniqueID()) + "_" + to_string(reinterpret_cast<uintptr_t>(_meshFilter));

    if (ImGui::BeginCombo(comboLabel.c_str(), currentName.c_str()))
    {
        if (ImGui::Selectable("None", currentMeshBuffer == nullptr))
            _meshFilter->Set_MeshBuffer(nullptr);

        for (_uint i = 0; i < meshOptions.size(); ++i)
        {
            const bool selected = (static_cast<_int>(i) == currentIndex);
            if (ImGui::Selectable(meshOptions[i].first.c_str(), selected))
                _meshFilter->Set_MeshBuffer(meshOptions[i].second);

            if (selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void CInspectorBox::ShowAddComponentMenu(CGameObject* _obj)
{
    if (!_obj)
        return;

    if (ImGui::Button("Add Component"))
        ImGui::OpenPopup("AddComponentPopup");

    if (!ImGui::BeginPopup("AddComponentPopup"))
        return;

    if (ImGui::MenuItem("Camera"))
    {
        if (!_obj->GetComponent<CCamera>())
            _obj->AddComponent<CCamera>();
    }

    if (ImGui::MenuItem("Light"))
    {
        if (!_obj->GetComponent<CLight>())
            _obj->AddComponent<CLight>();
    }

    if (ImGui::MenuItem("MeshRenderer"))
    {
        if (!_obj->GetComponent<CMeshRenderer>())
            _obj->AddComponent<CMeshRenderer>();
    }

    if (ImGui::MenuItem("MeshFilter"))
    {
        if (!_obj->GetComponent<CMeshFilter>())
            _obj->AddComponent<CMeshFilter>();
    }

    if (ImGui::MenuItem("SkinnedMeshRenderer"))
    {
        if (!_obj->GetComponent<CSkinnedMeshRenderer>())
            _obj->AddComponent<CSkinnedMeshRenderer>();
    }

    if (ImGui::MenuItem("Animator"))
    {
        if (!_obj->GetComponent<CAnimator>())
            _obj->AddComponent<CAnimator>();
    }

    if (ImGui::MenuItem("UI"))
    {
        if (!_obj->GetComponent<CUI>())
            _obj->AddComponent<CUI>();
    }

    if (ImGui::MenuItem("Canvas"))
    {
        if (!_obj->GetComponent<CCanvas>())
            _obj->AddComponent<CCanvas>();
    }

    if (ImGui::MenuItem("Terrain"))
    {
        if (!_obj->GetComponent<CTerrain>())
            _obj->AddComponent<CTerrain>();
    }

    if (ImGui::MenuItem("Transform"))
    {
        if (!_obj->GetComponent<CTransform>())
            _obj->AddComponent<CTransform>();
    }

    if (ImGui::MenuItem("RectTransform"))
    {
        if (!_obj->GetComponent<CRectTransform>())
        {
            CRectTransform* rectTransform = _obj->AddComponent<CRectTransform>();
            if (rectTransform)
                _obj->Set_Transform(rectTransform);
        }
    }

    ImGui::EndPopup();
}

static _bool IsPreviewImageExtension(const fs::path& path)
{
    string ext = CEditor::ToLowerCopy(path.extension().string());
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tif" || ext == ".tiff" || ext == ".gif";
}

static _bool TryGetAssetsRelativePath(const fs::path& path, wstring& outRel)
{
    const fs::path assetsRoot = fs::path(L"../Assets");
    error_code ec;
    fs::path absolutePath = fs::weakly_canonical(path, ec);
    fs::path absoluteRoot = fs::weakly_canonical(assetsRoot, ec);
    if (ec)
    {
        absolutePath = fs::absolute(path, ec);
        absoluteRoot = fs::absolute(assetsRoot, ec);
    }

    fs::path relative = absolutePath.lexically_relative(absoluteRoot);
    if (relative.empty() || relative.native().rfind(L"..", 0) == 0)
        return false;

    outRel = relative.wstring();
    outRel = CEngineString::Replace(outRel, L"\\", L"/");
    return true;
}

static string FormatFileSize(uintmax_t bytes)
{
    constexpr const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    double size = static_cast<double>(bytes);
    int unitIndex = 0;

    while (size >= 1024.0 && unitIndex < 4)
    {
        size /= 1024.0;
        ++unitIndex;
    }

    std::ostringstream oss;
    if (unitIndex == 0)
        oss << static_cast<uintmax_t>(size) << " " << units[unitIndex];
    else
        oss << std::fixed << std::setprecision(2) << size << " " << units[unitIndex];

    return oss.str();
}

void CInspectorBox::RenderSelectedAssetInfo(const fs::path& path)
{
    if (path.empty())
        return;

    error_code ec;
    if (!fs::exists(path, ec))
        return;

    const string name = path.filename().string();
    const string fullPath = path.string();
    const uintmax_t sizeBytes = fs::is_regular_file(path, ec) ? fs::file_size(path, ec) : 0;
    const string sizeText = (ec ? string("Unknown") : FormatFileSize(sizeBytes));

    _bool hasResolution = false;
    _uint width = 0;
    _uint height = 0;

    if (IsPreviewImageExtension(path))
    {
        wstring relPath;
        if (TryGetAssetsRelativePath(path, relPath))
        {
            const wstring pathKey = path.wstring();
            if (pathKey != m_previewAssetPath)
            {
                const wstring resourceName = L"InspectorPreview:" + pathKey;
                CResources& resources = CResources::GetInstance();
                auto found = resources.m_mGameResourceList.find(resourceName);
                if (found != resources.m_mGameResourceList.end())
                    m_pPreviewTexture = dynamic_cast<CTexture*>(found->second);
                else
                    m_pPreviewTexture = resources.CreateGameResource<CTexture>(resourceName, relPath);

                m_previewAssetPath = pathKey;
            }

            if (m_pPreviewTexture && m_pPreviewTexture->Get_SRV())
            {
                const D3D11_TEXTURE2D_DESC& desc = m_pPreviewTexture->Get_TextureDesc();
                if (desc.Width > 0 && desc.Height > 0)
                {
                    width = desc.Width;
                    height = desc.Height;
                    hasResolution = true;
                }
            }
        }
    }

    ImGui::Separator();
    ImGui::Text("Asset");

    if (ImGui::BeginTable("AssetInfoTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Value");

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Name");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(name.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Path");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", fullPath.c_str());

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Size");
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(sizeText.c_str());

        if (hasResolution)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted("Resolution");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u x %u", width, height);
        }

        ImGui::EndTable();
    }
}

void CInspectorBox::RenderSelectedAssetPreview(const fs::path& path)
{
    if (path.empty() || !IsPreviewImageExtension(path))
        return;

    wstring relPath;
    if (!TryGetAssetsRelativePath(path, relPath))
        return;

    const wstring pathKey = path.wstring();
    if (pathKey != m_previewAssetPath)
    {
        const wstring resourceName = L"InspectorPreview:" + pathKey;
        CResources& resources = CResources::GetInstance();
        auto found = resources.m_mGameResourceList.find(resourceName);
        if (found != resources.m_mGameResourceList.end())
            m_pPreviewTexture = dynamic_cast<CTexture*>(found->second);
        else
            m_pPreviewTexture = resources.CreateGameResource<CTexture>(resourceName, relPath);

        m_previewAssetPath = pathKey;
    }

    if (!m_pPreviewTexture || !m_pPreviewTexture->Get_SRV())
        return;

    const D3D11_TEXTURE2D_DESC& desc = m_pPreviewTexture->Get_TextureDesc();
    if (desc.Width == 0 || desc.Height == 0)
        return;

    ImGui::Separator();
    ImGui::Text("Preview");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 0.0f || avail.y <= 0.0f)
        return;

    float maxWidth = avail.x;
    float maxHeight = avail.y;
    float scale = std::min(maxWidth / static_cast<float>(desc.Width), maxHeight / static_cast<float>(desc.Height));
    scale = std::min(scale, 1.0f);

    ImVec2 size(static_cast<float>(desc.Width) * scale, static_cast<float>(desc.Height) * scale);
    ImTextureID texId = (ImTextureID)(intptr_t)m_pPreviewTexture->Get_SRV();
    ImGui::Image(ImTextureRef(texId), size);
}
