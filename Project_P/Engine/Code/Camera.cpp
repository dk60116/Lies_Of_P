#include "epch.h"
#include "Camera.h"
#include "Editor.h"
#include "MeshRenderer.h"
#include "SkinnedMeshRenderer.h"

namespace
{
	bool IsSameWorldMatrix(const _float4x4& lhs, const _float4x4& rhs)
	{
		return memcmp(&lhs, &rhs, sizeof(_float4x4)) == 0;
	}

	bool AreBoundingBoxesEquivalent(const BoundingBox& lhs, const BoundingBox& rhs)
	{
		const _float epsilon = 1e-4f;
		auto nearlyEqual = [epsilon](_float a, _float b)
		{
			return fabsf(a - b) <= epsilon;
		};

		return nearlyEqual(lhs.Center.x, rhs.Center.x)
			&& nearlyEqual(lhs.Center.y, rhs.Center.y)
			&& nearlyEqual(lhs.Center.z, rhs.Center.z)
			&& nearlyEqual(lhs.Extents.x, rhs.Extents.x)
			&& nearlyEqual(lhs.Extents.y, rhs.Extents.y)
			&& nearlyEqual(lhs.Extents.z, rhs.Extents.z);
	}

	_uint ResolveTextureMipLevelCount(const D3D11_TEXTURE2D_DESC& desc)
	{
		if (desc.MipLevels > 0)
			return desc.MipLevels;

		if (desc.Width == 0 || desc.Height == 0)
			return 0u;

		_uint mipLevels = 1u;
		_uint width = desc.Width;
		_uint height = desc.Height;

		while (width > 1u || height > 1u)
		{
			width = max(1u, width / 2u);
			height = max(1u, height / 2u);
			++mipLevels;
		}

		return mipLevels;
	}

	_bool TryProjectWorldAABBToViewport(const BoundingBox& worldAABB, CCamera* camera, const D3D11_VIEWPORT* viewport, _float& outWidthPx, _float& outHeightPx)
	{
		if (!camera || !viewport)
			return false;

		const auto clampFloat = [](_float value, const _float minValue, const _float maxValue)
		{
			return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
		};

		XMFLOAT3 corners[BoundingBox::CORNER_COUNT] = {};
		worldAABB.GetCorners(corners);

		const _matrix view = camera->GetViewMatrix();
		const _matrix proj = camera->GetProjectionMatrix();

		_float minX = FLT_MAX;
		_float minY = FLT_MAX;
		_float maxX = -FLT_MAX;
		_float maxY = -FLT_MAX;
		_bool hasProjectedPoint = false;

		for (const XMFLOAT3& corner : corners)
		{
			const _vector worldPos = XMVectorSet(corner.x, corner.y, corner.z, 1.f);
			const _vector viewPos = XMVector4Transform(worldPos, view);
			const _float viewZ = XMVectorGetZ(viewPos);
			if (!std::isfinite(viewZ) || viewZ <= 0.f)
				continue;

			const _vector clipPos = XMVector4Transform(viewPos, proj);
			const _float w = XMVectorGetW(clipPos);
			const _float safeW = (fabsf(w) <= 1e-6f) ? (w < 0.f ? -1e-6f : 1e-6f) : w;
			const _float xNDC = XMVectorGetX(clipPos) / safeW;
			const _float yNDC = XMVectorGetY(clipPos) / safeW;
			if (!std::isfinite(xNDC) || !std::isfinite(yNDC))
				continue;

			const _float pixelX = viewport->TopLeftX + (xNDC + 1.f) * 0.5f * viewport->Width;
			const _float pixelY = viewport->TopLeftY + (1.f - yNDC) * 0.5f * viewport->Height;

			minX = min(minX, pixelX);
			minY = min(minY, pixelY);
			maxX = max(maxX, pixelX);
			maxY = max(maxY, pixelY);
			hasProjectedPoint = true;
		}

		if (!hasProjectedPoint)
			return false;

		const _float viewportMinX = viewport->TopLeftX;
		const _float viewportMinY = viewport->TopLeftY;
		const _float viewportMaxX = viewport->TopLeftX + viewport->Width;
		const _float viewportMaxY = viewport->TopLeftY + viewport->Height;

		minX = clampFloat(minX, viewportMinX, viewportMaxX);
		minY = clampFloat(minY, viewportMinY, viewportMaxY);
		maxX = clampFloat(maxX, viewportMinX, viewportMaxX);
		maxY = clampFloat(maxY, viewportMinY, viewportMaxY);

		outWidthPx = (maxX > minX) ? (maxX - minX) : 0.f;
		outHeightPx = (maxY > minY) ? (maxY - minY) : 0.f;
		return outWidthPx > 0.f && outHeightPx > 0.f;
	}

	_float EstimateTextureMipLevelApprox(const D3D11_TEXTURE2D_DESC& desc, const _float2& tiling, const _float projectedWidthPx, const _float projectedHeightPx)
	{
		if (desc.Width == 0 || desc.Height == 0 || projectedWidthPx <= 0.f || projectedHeightPx <= 0.f)
			return 0.f;

		_float tileX = fabsf(tiling.x);
		_float tileY = fabsf(tiling.y);
		if (tileX <= 1e-4f)
			tileX = 1.f;
		if (tileY <= 1e-4f)
			tileY = 1.f;

		const _float safeProjectedWidth = max(projectedWidthPx, 1.f);
		const _float safeProjectedHeight = max(projectedHeightPx, 1.f);
		const _float texelsPerPixelX = (static_cast<_float>(desc.Width) * tileX) / safeProjectedWidth;
		const _float texelsPerPixelY = (static_cast<_float>(desc.Height) * tileY) / safeProjectedHeight;
		const _float texelsPerPixel = max(texelsPerPixelX, texelsPerPixelY);
		const _uint mipLevelCount = ResolveTextureMipLevelCount(desc);
		const _float maxMipLevel = (mipLevelCount > 0u) ? static_cast<_float>(mipLevelCount - 1u) : 0.f;
		const _float estimatedMip = log2f(max(texelsPerPixel, 1e-6f));

		return (estimatedMip < 0.f) ? 0.f : ((estimatedMip > maxMipLevel) ? maxMipLevel : estimatedMip);
	}

	void PopulateShadowCB(CCamera* camera, CLight* mainLight, const CLight::DirectionalShadowData& shadowData, ShadowCB& outShadowCB)
	{
		outShadowCB = {};

		const _uint cascadeCount = min<_uint>(shadowData.cascadeCount, kMaxShadowCascades);
		if (!camera || !mainLight || cascadeCount == 0u)
			return;

		for (_uint cascadeIndex = 0u; cascadeIndex < cascadeCount; ++cascadeIndex)
		{
			const _matrix lightView = XMLoadFloat4x4(&shadowData.cascades[cascadeIndex].matrices.view);
			const _matrix lightProj = XMLoadFloat4x4(&shadowData.cascades[cascadeIndex].matrices.proj);
			const _matrix lightViewProj = XMMatrixMultiply(lightView, lightProj);
			XMStoreFloat4x4(&outShadowCB.shadowViewProj[cascadeIndex], lightViewProj);
		}

		_float cascadeSplits[kMaxShadowCascades] = {};
		const _float lastSplitDepth = shadowData.cascades[cascadeCount - 1u].splitDepth;
		for (_uint cascadeIndex = 0u; cascadeIndex < kMaxShadowCascades; ++cascadeIndex)
		{
			cascadeSplits[cascadeIndex] = (cascadeIndex < cascadeCount)
				? shadowData.cascades[cascadeIndex].splitDepth
				: lastSplitDepth;
		}

		outShadowCB.cascadeSplits = _float4(
			cascadeSplits[0],
			cascadeSplits[1],
			cascadeSplits[2],
			cascadeSplits[3]);

		const _float shadowSize = static_cast<_float>(max<_uint>(CSceneManager::GetInstance().Get_LightSetting().shadowMapSize, 1u));
		const _float invShadowSize = 1.0f / shadowSize;
		outShadowCB.shadowParams = _float4(
			invShadowSize,
			invShadowSize,
			CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().shadowBias,
			CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().softShadowLightSize);

		const vector3 lightDir = mainLight->GetTransform()->Get_Directions().forward.normalized();
		outShadowCB.lightDirAndCount = _float4(
			lightDir.x,
			lightDir.y,
			lightDir.z,
			static_cast<_float>(cascadeCount));

		const vector3 cameraForward = camera->GetTransform()->Get_Directions().forward.normalized();
		outShadowCB.cameraForwardWS = _float4(
			cameraForward.x,
			cameraForward.y,
			cameraForward.z,
			std::clamp(CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().shadowCascadeBlendRatio, 0.f, 1.f));
	}

	struct RendererBatchKey
	{
		CMeshBuffer* meshBuffer = nullptr;
		CMaterial* material = nullptr;
		_bool isMirrored = false;

		bool operator==(const RendererBatchKey& rhs) const
		{
			return meshBuffer == rhs.meshBuffer
				&& material == rhs.material
				&& isMirrored == rhs.isMirrored;
		}
	};

	struct RendererBatchKeyHash
	{
		size_t operator()(const RendererBatchKey& key) const
		{
			const size_t h1 = hash<void*>()(static_cast<void*>(key.meshBuffer));
			const size_t h2 = hash<void*>()(static_cast<void*>(key.material));
			const size_t h3 = hash<int>()(static_cast<int>(key.isMirrored));
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	_bool PrepareInstancingChunk(CRenderer* leader, const vector<CRenderer*>& batch, size_t offset, const _uint maxInstanceCount)
	{
		if (!leader || !leader->GetTransform() || offset >= batch.size() || maxInstanceCount == 0)
			return false;

		vector<CRenderer*> chunkRenderers = {};
		chunkRenderers.reserve(min<size_t>(batch.size() - offset, maxInstanceCount));

		for (size_t i = offset; i < batch.size() && chunkRenderers.size() < maxInstanceCount; ++i)
		{
			CRenderer* renderer = batch[i];
			if (!renderer || !renderer->GetTransform())
				continue;

			chunkRenderers.push_back(renderer);
		}

	if (chunkRenderers.empty())
		return false;

	leader->CreateMeshInstancing(static_cast<_uint>(chunkRenderers.size()));

	for (_uint i = 0; i < static_cast<_uint>(chunkRenderers.size()); ++i)
	{
		CRenderer* renderer = chunkRenderers[i];
		leader->SetInstancingWorldMatrix(i, renderer->GetTransform()->GetSnapshotWorldMatrix());
	}

		return true;
	}

	UINT32 PackColorValue(const ColorValue& color)
	{
		return
			(static_cast<UINT32>(color.r)) |
			(static_cast<UINT32>(color.g) << 8) |
			(static_cast<UINT32>(color.b) << 16) |
			(static_cast<UINT32>(color.a) << 24);
	}

	struct UIImageBatchKey
	{
		CMeshBuffer* mesh = nullptr;
		CTexture* texture = nullptr;
		UINT32 color = 0;

		bool operator==(const UIImageBatchKey& rhs) const
		{
			return mesh == rhs.mesh
				&& texture == rhs.texture
				&& color == rhs.color;
		}
	};

	struct UIImageBatchKeyHash
	{
		size_t operator()(const UIImageBatchKey& key) const
		{
			const size_t h1 = hash<void*>()(static_cast<void*>(key.mesh));
			const size_t h2 = hash<void*>()(static_cast<void*>(key.texture));
			const size_t h3 = hash<UINT32>()(key.color);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};

	constexpr _int kInvalidUIImageBatchGroupId = -1;

	UIImageBatchKey BuildUIImageBatchKey(CImage* image)
	{
		UIImageBatchKey key = {};
		if (!image)
			return key;

		key.mesh = image->Get_Mesh();
		key.texture = image->GetTexture();
		key.color = PackColorValue(image->GetColor());
		return key;
	}

	struct UIBatchDebugAggregate
	{
		_uint batches = 0u;
		_uint instances = 0u;
		_uint singleBatches = 0u;
	};

	struct UILayeredBatchEntry
	{
		UIImageBatchKey key = {};
		vector<CImage*> images = {};
	};

	struct UIGroupedBatchEntry
	{
		UIImageBatchKey key = {};
		vector<CImage*> images = {};
	};

	_bool HasUIImageBatchGroup(CImage* image)
	{
		return image && image->GetGroupID() >= 0;
	}

	wstring BuildUIImageDebugKeyLabel(CImage* image)
	{
		if (!image)
			return L"<Null Image>";

		wstring label = L"Tex=";
		if (CTexture* texture = image->GetTexture())
			label += texture->Get_ResourceName();
		else
			label += L"<None>";

		const ColorValue color = image->GetColor();
		label += L" | RGBA(";
		label += to_wstring(static_cast<_uint>(color.r));
		label += L",";
		label += to_wstring(static_cast<_uint>(color.g));
		label += L",";
		label += to_wstring(static_cast<_uint>(color.b));
		label += L",";
		label += to_wstring(static_cast<_uint>(color.a));
		label += L")";

		if (HasUIImageBatchGroup(image))
		{
			label += L" | Group=";
			label += to_wstring(static_cast<_int>(image->GetGroupID()));
		}

		if (image->HasBatchLayer())
		{
			label += L" | Layer=";
			label += to_wstring(image->GetBatchLayer());
		}

		return label;
	}

	void RecordUIBatchDebugAggregate(
		unordered_map<wstring, UIBatchDebugAggregate>& aggregates,
		const vector<CImage*>& batch)
	{
		if (batch.empty())
			return;

		const wstring keyLabel = BuildUIImageDebugKeyLabel(batch.front());
		UIBatchDebugAggregate& aggregate = aggregates[keyLabel];
		aggregate.batches += 1u;
		aggregate.instances += static_cast<_uint>(batch.size());
		if (batch.size() == 1u)
			aggregate.singleBatches += 1u;
	}

	void FinalizeUIBatchDebugEntries(
		CCamera::RenderStats& stats,
		const unordered_map<wstring, UIBatchDebugAggregate>& aggregates)
	{
		stats.uiTopImageBatchEntries.clear();
		stats.uiTopImageBatchEntries.reserve(min<size_t>(aggregates.size(), 8u));

		for (const auto& [keyLabel, aggregate] : aggregates)
		{
			CCamera::RenderStats::UIBatchDebugEntry entry = {};
			entry.keyLabel = keyLabel;
			entry.batches = aggregate.batches;
			entry.instances = aggregate.instances;
			entry.singleBatches = aggregate.singleBatches;
			stats.uiTopImageBatchEntries.push_back(entry);
		}

		sort(
			stats.uiTopImageBatchEntries.begin(),
			stats.uiTopImageBatchEntries.end(),
			[](const CCamera::RenderStats::UIBatchDebugEntry& lhs, const CCamera::RenderStats::UIBatchDebugEntry& rhs)
			{
				if (lhs.batches != rhs.batches)
					return lhs.batches > rhs.batches;
				if (lhs.instances != rhs.instances)
					return lhs.instances > rhs.instances;
				if (lhs.singleBatches != rhs.singleBatches)
					return lhs.singleBatches > rhs.singleBatches;
				return lhs.keyLabel < rhs.keyLabel;
			});

		if (stats.uiTopImageBatchEntries.size() > 8u)
			stats.uiTopImageBatchEntries.resize(8u);
	}

	void QueueLayeredUIImage(
		map<_int, vector<UILayeredBatchEntry>>& layeredBatches,
		CImage* image)
	{
		if (!image)
			return;

		const _int layer = image->GetBatchLayer();
		vector<UILayeredBatchEntry>& layerEntries = layeredBatches[layer];
		const UIImageBatchKey key = BuildUIImageBatchKey(image);

		for (UILayeredBatchEntry& entry : layerEntries)
		{
			if (entry.key == key)
			{
				entry.images.push_back(image);
				return;
			}
		}

		UILayeredBatchEntry newEntry = {};
		newEntry.key = key;
		newEntry.images.push_back(image);
		layerEntries.push_back(newEntry);
	}

	void QueueGroupedUIImage(
		map<_int, vector<UIGroupedBatchEntry>>& groupedBatches,
		CImage* image)
	{
		if (!image)
			return;

		const _int groupId = image->GetGroupID();
		vector<UIGroupedBatchEntry>& groupEntries = groupedBatches[groupId];
		const UIImageBatchKey key = BuildUIImageBatchKey(image);

		for (UIGroupedBatchEntry& entry : groupEntries)
		{
			if (entry.key == key)
			{
				entry.images.push_back(image);
				return;
			}
		}

		UIGroupedBatchEntry newEntry = {};
		newEntry.key = key;
		newEntry.images.push_back(image);
		groupEntries.push_back(newEntry);
	}

	void BindUIInstanceBuffer(ID3D11DeviceContext* context, ID3D11Buffer* instanceBuffer, const vector<CImage*>& images)
	{
		if (!context || !instanceBuffer)
			return;

		InstanceCB cb = {};
		const size_t count = min<size_t>(images.size(), 128);

		for (size_t i = 0; i < count; ++i)
		{
			CImage* image = images[i];
			if (!image || !image->GetTransform())
				continue;

			cb.worlds[i] = XMMatrixTranspose(image->GetTransform()->Get_WorldMatrix());
			cb.fillParams[i] = _float4(
				image->GetFillAmount(),
				static_cast<_float>(image->Get_FillMethod()),
				static_cast<_float>(image->Get_FillOrigin()),
				image->Get_FillClockwise() ? 1.f : 0.f
			);
		}

		cb.instanceCount = static_cast<_uint>(count);
		context->UpdateSubresource(instanceBuffer, 0, nullptr, &cb, 0, 0);
		context->VSSetConstantBuffers(4, 1, &instanceBuffer);
		context->PSSetConstantBuffers(4, 1, &instanceBuffer);
	}

	void BindUIInstanceBufferEmpty(ID3D11DeviceContext* context, ID3D11Buffer* instanceBuffer)
	{
		if (!context || !instanceBuffer)
			return;

		InstanceCB cb = {};
		context->UpdateSubresource(instanceBuffer, 0, nullptr, &cb, 0, 0);
		context->VSSetConstantBuffers(4, 1, &instanceBuffer);
		context->PSSetConstantBuffers(4, 1, &instanceBuffer);
	}
}
const ColorValue CCamera::s_vDefaultCameraColor = ColorValue(49, 77, 121, 255);

CCamera::CCamera()
	: m_eClearFlag(ClearFlags::Skybox)
	, m_eCamViewMode(ViewMode::Perspective)
	, m_vViewMatrix()
	, m_vProjMatrix()
	, m_vVPInverseMatrix()
	, m_vBackgroundColor(s_vDefaultCameraColor)
	, m_iCullingMask(~0u)
	, m_fAspect(1.f)
	, m_fNear(0.1f)
	, m_fFar(600.f)
	, m_fFieldOfView(60.f)
	, m_fSize(5.f)
	, m_vStaticMeshList({})
	, m_vDynamicMeshEntries({})
	, m_vVisibleStaticMeshList({})
	, m_vVisibleDynamicMeshList({})
	, m_vUIList({})
	, m_sWorldFrustum()
	, m_sWorldOrthoBounds()
	, m_bUseOrthographicCulling(false)
	, m_mRTDebugDisplays({})
	, m_pRectBuffer(nullptr)
	, m_mRectMats({})
	, m_pRTDebugDS(nullptr)
	, m_pRTShadowDepthDS(nullptr)
	, m_pRTDebugRS(nullptr)
	, m_pRTShdowDepthRS(nullptr)
	, m_pRTDebugBS(nullptr)
	, m_pInvViewProjCB(nullptr)
	, m_pShadowCB(nullptr)
	, m_pUIInstanceBuffer(nullptr)
	, m_pMainLight(nullptr)
	, m_sMainLightShadowData()
	, m_pPickStaging(nullptr)
	, m_pStaticOctreeRoot(nullptr)
	, m_vStaticOctreeRenderers({})
	, m_mRendererBoundsCache({})
	, m_iOctreeMaxDepth(4)
	, m_iOctreeMaxEntriesPerNode(16)
	, m_bIsEditor(false)
{
	m_strName = L"Camera";
}

CCamera::~CCamera()
{
}

CCamera* CCamera::Create()
{
	return new CCamera();
}

CComponent* CCamera::Clone() const
{
	CCamera* clone = new CCamera();

	clone->m_eClearFlag = this->m_eClearFlag;
	clone->m_eCamViewMode = this->m_eCamViewMode;
	clone->m_vBackgroundColor = s_vDefaultCameraColor;
	clone->m_iCullingMask = this->m_iCullingMask;
	clone->m_fAspect = this->m_fAspect;
	clone->m_fNear = this->m_fNear;
	clone->m_fFar = this->m_fFar;
	clone->m_fFieldOfView = this->m_fFieldOfView;
	clone->m_fSize = this->m_fSize;

	return clone;
}

HRESULT CCamera::Initialize()
{
	if (FAILED(__super::Initialize()))
		return E_FAIL;

	m_bIsEditor = dynamic_cast<CEditorCamera*>(this);

	m_pRectBuffer = CResources::GetInstance().LoadOnGame<CMeshBuffer>(L"Rect (Mesh Buffer)");
	if (!m_pRectBuffer)
	{
		CDebug::LogError("Not found Rect (Mesh Buffer)");
		return E_FAIL;
	}

	CMaterial* presentMat = Add_RectMaterial(CRenderTarget::RTType::Present, L"DeferredPresent (Material)");
	CMaterial* ObjectPresentMat = Add_RectMaterial(CRenderTarget::RTType::ObjectPresent, L"ObjectIDPresent (Material)");
	CMaterial* depthPresentMat = Add_RectMaterial(CRenderTarget::RTType::Depth, L"DepthPresent (Material)");
	CMaterial* shadowDepthPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepthPresent, L"ShadowDepthPresent (Material)");
	CMaterial* shadowMaskPresentMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMaskPresent, L"ShadowMaskPresent (Material)");
	CMaterial* combineMat = Add_RectMaterial(CRenderTarget::RTType::Combine, L"DeferredCombine (Material)");
	CMaterial* shadowDepthMat = Add_RectMaterial(CRenderTarget::RTType::ShadowDepth, L"ShadowDepth (Material)");
	CMaterial* diffuseMat = Add_RectMaterial(CRenderTarget::RTType::Diffuse, L"DeferredDiffuse (Material)");
	CMaterial* specularMat = Add_RectMaterial(CRenderTarget::RTType::Specular, L"DeferredSpecular (Material)");
	CMaterial* shadowMaskMat = Add_RectMaterial(CRenderTarget::RTType::ShadowMask, L"ShadowMask (Material)");
	CMaterial* lightingCombinedMat = Add_RectMaterial(CRenderTarget::RTType::LightingCombined, L"DeferredLightingCombined (Material)");
	auto pushDisplay = [&](CRenderTarget::RTType type, CMaterial* mat)
		{
			RTDebugDisplay desc = {};
			desc.type = type;
			desc.quad = m_pRectBuffer;
			desc.material = mat;

			m_mRTDebugDisplays[type] = desc;
		};

	pushDisplay(CRenderTarget::RTType::Combine, presentMat);
	pushDisplay(CRenderTarget::RTType::Albedo, presentMat);
	pushDisplay(CRenderTarget::RTType::Object, ObjectPresentMat);
	pushDisplay(CRenderTarget::RTType::Normal, presentMat);
	pushDisplay(CRenderTarget::RTType::Material, presentMat);
	pushDisplay(CRenderTarget::RTType::Depth, depthPresentMat);
	pushDisplay(CRenderTarget::RTType::ShadowDepth, shadowDepthPresentMat);
	pushDisplay(CRenderTarget::RTType::Diffuse, presentMat);
	pushDisplay(CRenderTarget::RTType::Specular, presentMat);
	pushDisplay(CRenderTarget::RTType::ShadowMask, shadowMaskPresentMat);

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return E_FAIL;

	{
		D3D11_DEPTH_STENCIL_DESC ds = {};
		ds.DepthEnable = FALSE;
		ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
		ds.StencilEnable = FALSE;

		if (FAILED(device->CreateDepthStencilState(&ds, &m_pRTDebugDS)))
			return E_FAIL;
	}

	{
		D3D11_DEPTH_STENCIL_DESC sds = {};
		sds.DepthEnable = TRUE;
		sds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		sds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		sds.StencilEnable = FALSE;
		device->CreateDepthStencilState(&sds, &m_pRTShadowDepthDS);
	}

	{
		D3D11_RASTERIZER_DESC rs = {};
		rs.FillMode = D3D11_FILL_SOLID;
		rs.CullMode = D3D11_CULL_NONE;
		rs.FrontCounterClockwise = FALSE;
		rs.DepthClipEnable = TRUE;

		if (FAILED(device->CreateRasterizerState(&rs, &m_pRTDebugRS)))
			return E_FAIL;
	}

	{
		D3D11_RASTERIZER_DESC srs = {};
		srs.FillMode = D3D11_FILL_SOLID;
		// Keep shadow casting two-sided so backface-culled render surfaces still
		// participate in shadowing, while the depth test keeps the nearest blocker.
		srs.CullMode = D3D11_CULL_NONE;
		srs.FrontCounterClockwise = FALSE;
		srs.DepthClipEnable = TRUE;
		srs.DepthBias = 1000;
		srs.SlopeScaledDepthBias = 1.0f;
		srs.DepthBiasClamp = 0.f;

		if (FAILED(device->CreateRasterizerState(&srs, &m_pRTShdowDepthRS)))
			return E_FAIL;
	}

	{
		D3D11_BLEND_DESC bs = {};
		bs.AlphaToCoverageEnable = FALSE;
		bs.IndependentBlendEnable = FALSE;

		D3D11_RENDER_TARGET_BLEND_DESC rt = {};
		rt.BlendEnable = TRUE;
		rt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		rt.BlendOp = D3D11_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D11_BLEND_ONE;
		rt.DestBlendAlpha = D3D11_BLEND_ZERO;
		rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		bs.RenderTarget[0] = rt;

		if (FAILED(device->CreateBlendState(&bs, &m_pRTDebugBS)))
			return E_FAIL;
	}

	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(InvViewProjCB);
	if (FAILED(device->CreateBuffer(&bd, nullptr, &m_pInvViewProjCB)))
		return E_FAIL;
	m_pInvViewProjCB->AddRef();

	D3D11_BUFFER_DESC sbd = {};
	sbd.Usage = D3D11_USAGE_DEFAULT;
	sbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	sbd.ByteWidth = sizeof(ShadowCB);
	if (FAILED(device->CreateBuffer(&sbd, nullptr, &m_pShadowCB)))
		return E_FAIL;
	m_pShadowCB->AddRef();

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ibd.ByteWidth = sizeof(InstanceCB);
	if (FAILED(device->CreateBuffer(&ibd, nullptr, &m_pUIInstanceBuffer)))
		return E_FAIL;

	return S_OK;
}

void CCamera::Update()
{
	const D3D11_VIEWPORT* vp = ResolveViewport();

	if (vp && vp->Height > 0.f)
		m_fAspect = vp->Width / vp->Height;
	else
	{
		vector2Int res = m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution() : CDisplay::GetInstance().Get_ScreenResolution();
		m_fAspect = (res.y > 0) ? (_float)res.x / (_float)res.y : 1.f;
	}

	Find_MainLight();

	if (m_pMainLight)
		m_pMainLight->BuildDirectionalShadows(this, CSceneManager::GetInstance().Get_CrtScene()->Get_EnviromentSetting().directionalLightShadowDist, m_sMainLightShadowData);
	else
		m_sMainLightShadowData = {};

	Bind_ViewMatrix();
	Bind_ProjectionMatrix();
	Update_WorldFrustum();
}

void CCamera::Render()
{
}

void CCamera::OnPostRender()
{
	m_vStaticMeshList.clear();
	m_vDynamicMeshEntries.clear();
	m_vVisibleStaticMeshList.clear();
	m_vVisibleStaticMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.clear();
	m_vVisibleDynamicMeshList_Transparent.clear();
}

void CCamera::OnDestroy()
{
	m_mRectMats.clear();

	m_mRTDebugDisplays.clear();
	m_pStaticOctreeRoot.reset();
	m_vStaticOctreeRenderers.clear();
	m_mRendererBoundsCache.clear();

	Safe_Release(m_pRTDebugDS);
	Safe_Release(m_pRTShadowDepthDS);
	Safe_Release(m_pRTDebugRS);
	Safe_Release(m_pRTShdowDepthRS);
	Safe_Release(m_pRTDebugBS);

	Safe_Release(m_pInvViewProjCB);
	Safe_Release(m_pShadowCB);
	Safe_Release(m_pUIInstanceBuffer);

	Safe_Release(m_pPickStaging);
	Safe_Release(m_pMainLight);

	if (auto scene = CSceneManager::GetInstance().Get_CrtScene())
		scene->Remove_Camera(this);
}

_matrix CCamera::GetViewMatrix() const
{
	_matrix result = XMLoadFloat4x4(&m_vViewMatrix);
	return result;
}

_matrix CCamera::GetProjectionMatrix() const
{
	_matrix result = XMLoadFloat4x4(&m_vProjMatrix);
	return result;
}

const CCamera::ClearFlags CCamera::GetClearFlags() const
{
	return m_eClearFlag;
}

void CCamera::SetClearFlags(const ClearFlags _flag)
{
	m_eClearFlag = _flag;
}

const CCamera::ViewMode CCamera::GetViewMode() const
{
	return m_eCamViewMode;
}

void CCamera::SetViewMode(const ViewMode _mode)
{
	m_eCamViewMode = _mode;
}

const _uint CCamera::GetCullingMask() const
{
	return m_iCullingMask;
}

void CCamera::SetCullingMask(const _uint _mask)
{
	m_iCullingMask = _mask;
}

const _float CCamera::GetAspect() const
{
	return m_fAspect;
}

const _float CCamera::GetNear() const
{
	return m_fNear;
}

void CCamera::SetNear(const _float _value)
{
	m_fNear = max(0.001f, _value);
	if (m_fFar <= m_fNear)
		m_fFar = m_fNear + 0.001f;
}

const _float CCamera::GetFar() const
{
	return m_fFar;
}

void CCamera::SetFar(const _float _value)
{
	m_fFar = max(m_fNear + 0.001f, _value);
}

const _float CCamera::GetFieldOfView() const
{
	return m_fFieldOfView;
}

void CCamera::SetFieldOfView(const _float _value)
{
	m_fFieldOfView = clamp(_value, 1.f, 179.f);
}

const _float CCamera::GetOrthographicSize() const
{
	return m_fSize;
}

void CCamera::SetOrthographicSize(const _float _value)
{
	m_fSize = max(0.001f, _value);
}

const ColorValue& CCamera::Get_BackgroundColor() const
{
	return m_vBackgroundColor;
}

void CCamera::SetBackgroundColor(const ColorValue& _color)
{
	m_vBackgroundColor = _color;
}

const CCamera::RenderStats& CCamera::GetRenderStats() const
{
	return m_sRenderStats;
}

void CCamera::ResetRenderStats()
{
	m_sRenderStats = {};
}

void CCamera::AccumulateRenderStats(CRenderer* _renderer, _uint _instanceCount)
{
	if (!_renderer || _instanceCount == 0u)
		return;

	CMeshBuffer* meshBuffer = _renderer->Get_MeshBuffer();
	if (!meshBuffer)
		return;

	const CMeshBuffer::MESHBUFFERDESC& info = meshBuffer->Get_Info();
	const _uint trisPerInstance = (info.indexCount > 0u) ? (info.indexCount / 3u) : (info.vertextCount / 3u);

	m_sRenderStats.batches += 1u;
	m_sRenderStats.tris += trisPerInstance * _instanceCount;
	m_sRenderStats.verts += info.vertextCount * _instanceCount;

	if (dynamic_cast<CSkinnedMeshRenderer*>(_renderer))
		m_sRenderStats.visibleSkinnedMeshes += _instanceCount;
}

void CCamera::Add_RenderTarget_Mesh(CRenderer* _mesh)
{
	if (!_mesh || !_mesh->Get_GameObject())
		return;

	if (!CSceneManager::GetInstance().ContainLayerMask(_mesh->Get_GameObject()->GetLayer(), m_iCullingMask))
		return;

	if (_mesh->Get_GameObject()->IsStatic(CGameObject::STATIC_METHOD::TransformStatic))
		m_vStaticMeshList.push_back(_mesh);
	else
		m_vDynamicMeshEntries.push_back({ _mesh, &m_mRendererBoundsCache[_mesh] });
}

void CCamera::Add_RenderTarget_UI(CUI* _ui)
{
	if (!_ui || !_ui->Get_GameObject())
		return;

	if (!CSceneManager::GetInstance().ContainLayerMask(_ui->Get_GameObject()->GetLayer(), m_iCullingMask))
		return;

	m_vUIList.push_back(_ui);
}

const vector2 CCamera::WorldToScreenPoint(const vector3& _world) const
{
	const D3D11_VIEWPORT* vp = ResolveViewport();
	const _float W = vp ? vp->Width  : (_float)(m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution().x : CDisplay::GetInstance().Get_ScreenResolution().x);
	const _float H = vp ? vp->Height : (_float)(m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution().y : CDisplay::GetInstance().Get_ScreenResolution().y);

	_matrix view = XMLoadFloat4x4(&m_vViewMatrix);
	_matrix proj = XMLoadFloat4x4(&m_vProjMatrix);

	_vector worldPos = XMVectorSet(_world.x, _world.y, _world.z, 1.f);
	_vector viewPos = XMVector4Transform(worldPos, view);
	const _float viewZ = XMVectorGetZ(viewPos);

	if (!std::isfinite(viewZ) || viewZ <= 0.f)
		return vector2(-FLT_MAX, -FLT_MAX);

	_vector clipPos = XMVector4Transform(viewPos, proj);

	const _float w = XMVectorGetW(clipPos);
	const _float safeW = (fabsf(w) <= 1e-6f) ? (w < 0.f ? -1e-6f : 1e-6f) : w;

	_float xNDC = XMVectorGetX(clipPos) / safeW;
	_float yNDC = XMVectorGetY(clipPos) / safeW;

	if (!std::isfinite(xNDC))
		xNDC = xNDC < 0.f ? -FLT_MAX : FLT_MAX;

	if (!std::isfinite(yNDC))
		yNDC = yNDC < 0.f ? -FLT_MAX : FLT_MAX;

	return vector2
	(
		(xNDC + 1.f) * 0.5f * W,
		(1.f - yNDC) * 0.5f * H
	);
}

void CCamera::Bind_ViewMatrix()
{
	_matrix inverseWorldMatrix = GetTransform()->Get_InverseWorldMatrix();
	XMStoreFloat4x4(&m_vViewMatrix, inverseWorldMatrix);
}

void CCamera::Bind_ProjectionMatrix()
{
	switch (m_eCamViewMode)
	{
	case CCamera::ViewMode::Perspective:
	{
		_matrix projMat = XMMatrixPerspectiveFovLH
		(
			XMConvertToRadians(m_fFieldOfView),
			m_fAspect,
			m_fNear, m_fFar
		);

		XMStoreFloat4x4(&m_vProjMatrix, projMat);
	}
	break;

	case CCamera::ViewMode::Orthographic:
	{
		const _float fHalfHeight = m_fSize * 0.5f;
		const _float fHalfWidth = fHalfHeight * m_fAspect;

		_matrix projMat = XMMatrixOrthographicOffCenterLH
		(
			-fHalfWidth, fHalfWidth,
			-fHalfHeight, fHalfHeight,
			m_fNear, m_fFar
		);

		XMStoreFloat4x4(&m_vProjMatrix, projMat);
	}
	break;

	default:
		break;
	}

	_matrix inv = XMMatrixInverse(nullptr, GetViewMatrix() * GetProjectionMatrix());
	XMStoreFloat4x4(&m_vVPInverseMatrix, inv);
}

void CCamera::RenderMesh()
{
	Collect_VisibleRenderers();
	ResetRenderStats();

	CCamera* selectedPreviewCamera = nullptr;
	CMaterial* textureLODPreviewMaterial = nullptr;
	if (m_bIsEditor)
	{
		CCamera* selectedCamera = CEditor::GetInstance().Get_SelectedCamera();
		if (selectedCamera && selectedCamera != this && selectedCamera->Get_GameObject() && selectedCamera->Get_GameObject()->IsRecursiveActive() && selectedCamera->Get_Enable())
		{
			selectedPreviewCamera = selectedCamera;
			textureLODPreviewMaterial = CResources::GetInstance().LoadOnGame<CMaterial>(L"TextureLODPreview (Material)");
		}
	}

	const auto renderWithOptionalPreview = [&](CRenderer* renderer, const _bool allowPreview)
	{
		if (!renderer || !renderer->Get_GameObject())
			return;
		if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable() || !renderer->IsLODVisible())
			return;

		_bool renderedPreview = false;
		if (allowPreview && selectedPreviewCamera && textureLODPreviewMaterial)
		{
			CMaterial* originalMaterial = renderer->Get_Material();
			CTexture* baseTexture = originalMaterial ? originalMaterial->Get_Texture(0) : nullptr;
			const D3D11_VIEWPORT* previewViewport = nullptr;

			if (selectedPreviewCamera->m_bIsEditor)
				previewViewport = CGraphicDevice::GetInstance().Get_EditorViewport();
			else
				previewViewport = CGraphicDevice::GetInstance().Get_GameViewport();

			if (!previewViewport)
				previewViewport = CGraphicDevice::GetInstance().Get_CurrentViewport();

			if (originalMaterial && !originalMaterial->IsTransparnet() && baseTexture && baseTexture->Get_SRV() && previewViewport)
			{
				BoundingBox worldAABB = {};
				_float projectedWidthPx = 0.f;
				_float projectedHeightPx = 0.f;
				if (selectedPreviewCamera->TryBuildRendererWorldAABB(renderer, worldAABB)
					&& TryProjectWorldAABBToViewport(worldAABB, selectedPreviewCamera, previewViewport, projectedWidthPx, projectedHeightPx))
				{
					_float2 tiling = { 1.f, 1.f };
					const auto& vector2Values = originalMaterial->Get_Vector2Values();
					const auto foundTiling = vector2Values.find(L"gTiling");
					if (foundTiling != vector2Values.end())
						tiling = foundTiling->second;

					const _float previewMip = EstimateTextureMipLevelApprox(baseTexture->Get_TextureDesc(), tiling, projectedWidthPx, projectedHeightPx);
					textureLODPreviewMaterial->Set_Texture(baseTexture, 0);
					textureLODPreviewMaterial->Set_BaseColor(originalMaterial->Get_BaseColor());
					textureLODPreviewMaterial->Set_Vector4Value(L"gPreviewParams", _float4(previewMip, tiling.x, tiling.y, 0.f));
					renderer->Render_WithCameraOverrideMaterial(this, textureLODPreviewMaterial);
					renderedPreview = true;
				}
			}
		}

		if (!renderedPreview)
			renderer->Render_WithCamera(this);

		AccumulateRenderStats(renderer);
	};

	if (!m_bIsEditor)
	{
		unordered_map<RendererBatchKey, vector<CRenderer*>, RendererBatchKeyHash> staticBatches = {};
		staticBatches.reserve(m_vVisibleStaticMeshList.size());

		vector<CRenderer*> nonBatchedStatic = {};
		nonBatchedStatic.reserve(m_vVisibleStaticMeshList.size());

		for (auto* renderer : m_vVisibleStaticMeshList)
		{
			if (!renderer || !renderer->Get_GameObject())
				continue;
			if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable() || !renderer->IsLODVisible())
				continue;

			auto* meshRenderer = dynamic_cast<CMeshRenderer*>(renderer);
			CMaterial* material = renderer->Get_Material();
			CMeshBuffer* meshBuffer = renderer->Get_MeshBuffer();
			if (!meshRenderer || !material || material->IsTransparnet() || !meshBuffer)
			{
				nonBatchedStatic.push_back(renderer);
				continue;
			}

			RendererBatchKey key = { meshBuffer, material, renderer->IsMirroredTransform() };
			staticBatches[key].push_back(renderer);
		}

		for (auto* renderer : nonBatchedStatic)
			renderWithOptionalPreview(renderer, false);

		for (auto& kv : staticBatches)
		{
			auto& batch = kv.second;
			if (batch.empty())
				continue;

			CRenderer* leader = batch[0];
			if (!leader || !leader->Get_GameObject() || !leader->GetTransform())
				continue;

			if (batch.size() == 1)
			{
				renderWithOptionalPreview(leader, false);
				continue;
			}

			const _uint maxInstanceCount = 128u;
			for (size_t offset = 0; offset < batch.size(); offset += maxInstanceCount)
			{
				if (!PrepareInstancingChunk(leader, batch, offset, maxInstanceCount))
					continue;

				const _uint instanceCount = static_cast<_uint>(min<size_t>(batch.size() - offset, maxInstanceCount));
				leader->Render_WithCamera(this);
				AccumulateRenderStats(leader, instanceCount);
				leader->CreateMeshInstancing(0);
			}
		}
	}
	else
	{
		for (TRAVERSAL_ITER(m_vVisibleStaticMeshList, it))
			renderWithOptionalPreview(*it, true);
	}

	for (TRAVERSAL_ITER(m_vVisibleDynamicMeshList, it))
		renderWithOptionalPreview(*it, m_bIsEditor);

	SortTransparentRenderersByCameraDistance(m_vVisibleStaticMeshList_Transparent);
	SortTransparentRenderersByCameraDistance(m_vVisibleDynamicMeshList_Transparent);

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	CScene* scene = CSceneManager::GetInstance().Get_CrtScene();
	if (!context || !scene)
		return;

	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;
	context->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	context->OMGetDepthStencilState(&prevDS, &prevStencilRef);

	const _float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	context->OMSetBlendState(scene->Get_BlendingState(), blendFactor, 0xFFFFFFFF);
	context->OMSetDepthStencilState(scene->Get_TransparentDepthStencillState(), prevStencilRef);

	for (TRAVERSAL_ITER(m_vVisibleStaticMeshList_Transparent, it))
		renderWithOptionalPreview(*it, false);

	for (TRAVERSAL_ITER(m_vVisibleDynamicMeshList_Transparent, it))
		renderWithOptionalPreview(*it, false);

	context->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);
	context->OMSetDepthStencilState(prevDS, prevStencilRef);
	Safe_Release(prevBS);
	Safe_Release(prevDS);
}
void CCamera::Update_WorldFrustum()
{
	_matrix cullingView = GetViewMatrix();
	_matrix cullingProj = GetProjectionMatrix();
	ViewMode cullingViewMode = m_eCamViewMode;
	_float cullingAspect = m_fAspect;
	_float cullingNear = m_fNear;
	_float cullingFar = m_fFar;
	_float cullingSize = m_fSize;

	if (m_bIsEditor)
	{
		if (CGameObject* selected = CEditor::GetInstance().Get_SelectedGameObject())
		{
			if (CCamera* selectedCamera = selected->GetComponent<CCamera>())
			{
				cullingView = selectedCamera->GetViewMatrix();
				cullingProj = selectedCamera->GetProjectionMatrix();
				cullingViewMode = selectedCamera->GetViewMode();
				cullingAspect = selectedCamera->GetAspect();
				cullingNear = selectedCamera->GetNear();
				cullingFar = selectedCamera->GetFar();
				cullingSize = selectedCamera->GetOrthographicSize();
			}
		}
	}

	_matrix invView = XMMatrixInverse(nullptr, cullingView);
	if (cullingViewMode == ViewMode::Orthographic)
	{
		const _float halfHeight = max(cullingSize * 0.5f, 0.001f);
		const _float halfWidth = max(halfHeight * cullingAspect, 0.001f);
		const _float halfDepth = max((cullingFar - cullingNear) * 0.5f, 0.001f);
		const _float centerZ = cullingNear + halfDepth;

		BoundingOrientedBox localBounds = {};
		localBounds.Center = XMFLOAT3(0.f, 0.f, centerZ);
		localBounds.Extents = XMFLOAT3(halfWidth, halfHeight, halfDepth);
		localBounds.Orientation = XMFLOAT4(0.f, 0.f, 0.f, 1.f);

		m_sWorldOrthoBounds = localBounds;
		m_sWorldOrthoBounds.Transform(m_sWorldOrthoBounds, invView);
		m_bUseOrthographicCulling = true;
		return;
	}

	BoundingFrustum localFrustum = {};
	BoundingFrustum::CreateFromMatrix(localFrustum, cullingProj);
	m_sWorldFrustum = localFrustum;
	m_sWorldFrustum.Transform(m_sWorldFrustum, invView);
	m_bUseOrthographicCulling = false;
}

_bool CCamera::TryBuildRendererWorldAABB(CRenderer* _renderer, RendererBoundsCache& _cache, BoundingBox& _outAABB, _bool* _outChanged) const
{
	if (_outChanged)
		*_outChanged = false;

	if (!_renderer || !_renderer->Get_GameObject() || !_renderer->GetTransform())
		return false;

	CMeshBuffer* meshBuffer = _renderer->Get_MeshBuffer();
	if (!meshBuffer)
		return false;

	if (CSkinnedMeshRenderer* skinned = dynamic_cast<CSkinnedMeshRenderer*>(_renderer))
	{
		_float3 minBound = {};
		_float3 maxBound = {};
		if (skinned->TryGetAnimatedWorldBounds(minBound, maxBound))
		{
			BoundingBox::CreateFromPoints(_outAABB, XMLoadFloat3(&minBound), XMLoadFloat3(&maxBound));
			if (_outChanged)
				*_outChanged = true;
			return true;
		}
	}

	_float4x4 worldMatrix = {};
	XMStoreFloat4x4(&worldMatrix, _renderer->GetTransform()->GetSnapshotWorldMatrix());

	if (_cache.valid && _cache.meshBuffer == meshBuffer && IsSameWorldMatrix(_cache.worldMatrix, worldMatrix))
	{
		_outAABB = _cache.worldAABB;
		return true;
	}

	const BoundingBox& localBox = meshBuffer->Get_Info().boundingBox;
	BoundingOrientedBox localObb = {};
	BoundingOrientedBox::CreateFromBoundingBox(localObb, localBox);

	BoundingOrientedBox worldObb = {};
	localObb.Transform(worldObb, XMLoadFloat4x4(&worldMatrix));

	XMFLOAT3 corners[8] = {};
	worldObb.GetCorners(corners);

	_vector minV = XMLoadFloat3(&corners[0]);
	_vector maxV = minV;
	for (_int i = 1; i < 8; ++i)
	{
		_vector p = XMLoadFloat3(&corners[i]);
		minV = XMVectorMin(minV, p);
		maxV = XMVectorMax(maxV, p);
	}

	BoundingBox::CreateFromPoints(_outAABB, minV, maxV);

	const _bool changed = !_cache.valid
		|| _cache.meshBuffer != meshBuffer
		|| !AreBoundingBoxesEquivalent(_cache.worldAABB, _outAABB);

	_cache.worldAABB = _outAABB;
	_cache.worldMatrix = worldMatrix;
	_cache.meshBuffer = meshBuffer;
	_cache.valid = true;

	if (_outChanged)
		*_outChanged = changed;

	return true;
}

_bool CCamera::TryBuildRendererWorldAABB(CRenderer* _renderer, BoundingBox& _outAABB, _bool* _outChanged) const
{
	return TryBuildRendererWorldAABB(_renderer, m_mRendererBoundsCache[_renderer], _outAABB, _outChanged);
}

_bool CCamera::IsRendererVisible(CRenderer* _renderer) const
{
	BoundingBox worldAABB = {};
	if (!TryBuildRendererWorldAABB(_renderer, worldAABB))
		return false;

	ContainmentType contain = m_bUseOrthographicCulling
		? m_sWorldOrthoBounds.Contains(worldAABB)
		: m_sWorldFrustum.Contains(worldAABB);
	return contain != ContainmentType::DISJOINT;
}

_bool CCamera::IsRendererVisible(CRenderer* _renderer, RendererBoundsCache& _cache) const
{
	BoundingBox worldAABB = {};
	if (!TryBuildRendererWorldAABB(_renderer, _cache, worldAABB))
		return false;

	ContainmentType contain = m_bUseOrthographicCulling
		? m_sWorldOrthoBounds.Contains(worldAABB)
		: m_sWorldFrustum.Contains(worldAABB);
	return contain != ContainmentType::DISJOINT;
}

_bool CCamera::IsOctreeNodeLeaf(const OctreeNode* _node) const
{
	if (!_node)
		return true;

	for (const auto& child : _node->children)
	{
		if (child)
			return false;
	}
	return true;
}

void CCamera::InsertStaticOctreeEntry(OctreeNode* _node, const OctreeEntry& _entry)
{
	if (!_node)
		return;

	if (_node->depth >= m_iOctreeMaxDepth)
	{
		_node->entries.push_back(_entry);
		return;
	}

	const _float3& c = _node->bounds.Center;
	const _float3& e = _node->bounds.Extents;
	_float3 childExtent = { e.x * 0.5f, e.y * 0.5f, e.z * 0.5f };

	_int fitChild = -1;
	for (_int i = 0; i < 8; ++i)
	{
		_float3 sign =
		{
			(i & 1) ? 0.5f : -0.5f,
			(i & 2) ? 0.5f : -0.5f,
			(i & 4) ? 0.5f : -0.5f
		};
		_float3 childCenter =
		{
			c.x + e.x * sign.x,
			c.y + e.y * sign.y,
			c.z + e.z * sign.z
		};

		BoundingBox childBox = {};
		childBox.Center = childCenter;
		childBox.Extents = childExtent;

		ContainmentType contain = childBox.Contains(_entry.worldAABB);
		if (contain == ContainmentType::CONTAINS)
		{
			fitChild = i;
			break;
		}
	}

	if (fitChild < 0)
	{
		_node->entries.push_back(_entry);
		return;
	}

	if (!_node->children[fitChild])
	{
		_node->children[fitChild] = make_unique<OctreeNode>();
		_node->children[fitChild]->depth = _node->depth + 1;

		_float3 sign =
		{
			(fitChild & 1) ? 0.5f : -0.5f,
			(fitChild & 2) ? 0.5f : -0.5f,
			(fitChild & 4) ? 0.5f : -0.5f
		};
		_node->children[fitChild]->bounds.Center =
		{
			c.x + e.x * sign.x,
			c.y + e.y * sign.y,
			c.z + e.z * sign.z
		};
		_node->children[fitChild]->bounds.Extents = childExtent;
	}

	InsertStaticOctreeEntry(_node->children[fitChild].get(), _entry);
}

void CCamera::BuildStaticOctree()
{
	if (m_vStaticMeshList.empty())
	{
		m_pStaticOctreeRoot.reset();
		m_vStaticOctreeRenderers.clear();
		return;
	}

	vector<CRenderer*> currentStaticRenderers = {};
	currentStaticRenderers.reserve(m_vStaticMeshList.size());

	vector<OctreeEntry> entries = {};
	entries.reserve(m_vStaticMeshList.size());

	_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
	_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
	_bool boundsChanged = false;

	for (auto* renderer : m_vStaticMeshList)
	{
		if (!renderer || !renderer->Get_GameObject())
			continue;
		if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable())
			continue;

		if (!CSceneManager::GetInstance().ContainLayerMask(renderer->Get_GameObject()->GetLayer(), m_iCullingMask))
			continue;

		currentStaticRenderers.push_back(renderer);

		BoundingBox worldAABB = {};
		_bool rendererChanged = false;
		if (!TryBuildRendererWorldAABB(renderer, worldAABB, &rendererChanged))
			continue;

		boundsChanged = boundsChanged || rendererChanged;
		entries.push_back({ renderer, worldAABB });

		_vector aabbMin = XMVectorSet(worldAABB.Center.x - worldAABB.Extents.x, worldAABB.Center.y - worldAABB.Extents.y, worldAABB.Center.z - worldAABB.Extents.z, 0.f);
		_vector aabbMax = XMVectorSet(worldAABB.Center.x + worldAABB.Extents.x, worldAABB.Center.y + worldAABB.Extents.y, worldAABB.Center.z + worldAABB.Extents.z, 0.f);
		minV = XMVectorMin(minV, aabbMin);
		maxV = XMVectorMax(maxV, aabbMax);
	}

	const _bool rendererSetChanged = m_vStaticOctreeRenderers.size() != currentStaticRenderers.size()
		|| !equal(m_vStaticOctreeRenderers.begin(), m_vStaticOctreeRenderers.end(), currentStaticRenderers.begin());

	if (!rendererSetChanged && !boundsChanged && m_pStaticOctreeRoot)
		return;

	m_pStaticOctreeRoot.reset();
	m_vStaticOctreeRenderers = currentStaticRenderers;

	if (entries.empty())
		return;

	BoundingBox rootBounds = {};
	BoundingBox::CreateFromPoints(rootBounds, minV, maxV);

	_float maxExtent = max(rootBounds.Extents.x, max(rootBounds.Extents.y, rootBounds.Extents.z));
	rootBounds.Extents = { maxExtent, maxExtent, maxExtent };

	m_pStaticOctreeRoot = make_unique<OctreeNode>();
	m_pStaticOctreeRoot->bounds = rootBounds;
	m_pStaticOctreeRoot->depth = 0;

	for (const auto& entry : entries)
		InsertStaticOctreeEntry(m_pStaticOctreeRoot.get(), entry);
}

void CCamera::QueryStaticOctree(const OctreeNode* _node, vector<CRenderer*>& _outVisible) const
{
	if (!_node)
		return;

	ContainmentType contain = m_bUseOrthographicCulling
		? m_sWorldOrthoBounds.Contains(_node->bounds)
		: m_sWorldFrustum.Contains(_node->bounds);
	if (contain == ContainmentType::DISJOINT)
		return;

	if (contain == ContainmentType::CONTAINS)
	{
		for (const auto& entry : _node->entries)
			_outVisible.push_back(entry.renderer);

		for (const auto& child : _node->children)
		{
			if (!child)
				continue;
			QueryStaticOctree(child.get(), _outVisible);
		}
		return;
	}

	for (const auto& entry : _node->entries)
	{
		ContainmentType entryContain = m_bUseOrthographicCulling
			? m_sWorldOrthoBounds.Contains(entry.worldAABB)
			: m_sWorldFrustum.Contains(entry.worldAABB);
		if (entryContain != ContainmentType::DISJOINT)
			_outVisible.push_back(entry.renderer);
	}

	for (const auto& child : _node->children)
	{
		if (!child)
			continue;
		QueryStaticOctree(child.get(), _outVisible);
	}
}

void CCamera::SortTransparentRenderersByCameraDistance(vector<CRenderer*>& _renderers)
{
	if (_renderers.empty())
		return;

	vector3 cameraPos = GetTransform()->Get_Position();

	sort(_renderers.begin(), _renderers.end(), [&](CRenderer* _lhs, CRenderer* _rhs)
		{
			if (!_lhs || !_rhs)
				return _lhs != nullptr;

			vector3 lhsPos = _lhs->GetTransform()->Get_Position();
			vector3 rhsPos = _rhs->GetTransform()->Get_Position();

			const _float lhsDistSq = (lhsPos - cameraPos).lengthSq();
			const _float rhsDistSq = (rhsPos - cameraPos).lengthSq();

			return lhsDistSq > rhsDistSq;
		});
}

void CCamera::Collect_VisibleRenderers()
{
	m_vVisibleStaticMeshList.clear();
	m_vVisibleStaticMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.clear();
	m_vVisibleDynamicMeshList_Transparent.clear();
	m_vVisibleDynamicMeshList.reserve(m_vDynamicMeshEntries.size());

	BuildStaticOctree();
	if (m_pStaticOctreeRoot)
		QueryStaticOctree(m_pStaticOctreeRoot.get(), m_vVisibleStaticMeshList);

	vector<CRenderer*> opaqueStatic = {};
	opaqueStatic.reserve(m_vVisibleStaticMeshList.size());

	for (auto* renderer : m_vVisibleStaticMeshList)
	{
		if (!renderer || !renderer->Get_GameObject() || !renderer->Get_Material())
			continue;

		if (!CSceneManager::GetInstance().ContainLayerMask(renderer->Get_GameObject()->GetLayer(), m_iCullingMask))
			continue;

		if (!renderer->Get_Material()->IsTransparnet())
			opaqueStatic.push_back(renderer);
		else
			m_vVisibleStaticMeshList_Transparent.push_back(renderer);
	}

	m_vVisibleStaticMeshList.swap(opaqueStatic);

	for (auto& entry : m_vDynamicMeshEntries)
	{
		auto* renderer = entry.renderer;
		if (!renderer || !renderer->Get_GameObject())
			continue;

		if (!renderer->Get_GameObject()->IsRecursiveActive() || !renderer->Get_Enable())
			continue;

		if (!CSceneManager::GetInstance().ContainLayerMask(renderer->Get_GameObject()->GetLayer(), m_iCullingMask))
			continue;

		if (!renderer->Get_Material())
			continue;

		if (IsRendererVisible(renderer, *entry.cache))
		{
			if (!renderer->Get_Material()->IsTransparnet())
				m_vVisibleDynamicMeshList.push_back(renderer);
			else
				m_vVisibleDynamicMeshList_Transparent.push_back(renderer);
		}
	}
}

void CCamera::RenderUI()
{
	_matrix viewMat = XMMatrixTranslation(50.f, 50.f, 0.f);

	_vector det = {};
	const _matrix inverseMat = XMMatrixInverse(&det, viewMat);

	_float4 camPosF4;
	XMStoreFloat4(&camPosF4, inverseMat.r[3]);

	const _float aspect = CDisplay::GetInstance().Get_Aspect();
	const _float fHalfHeight = 7.2f * 0.5f;
	const _float fHalfWidth = fHalfHeight * aspect;

	const _matrix projMat = XMMatrixOrthographicOffCenterLH
	(
		-fHalfWidth, fHalfWidth,
		-fHalfHeight, fHalfHeight,
		0.f, 1.f
	);

	if (m_vUIList.size() <= 0)
		return;

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	if (!context)
		return;

	BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);

	vector<CImage*> imageBatch = {};
	vector<CImage*> deferredImages = {};
	map<_int, vector<UIGroupedBatchEntry>> groupedImageBatches = {};
	map<_int, vector<UILayeredBatchEntry>> layeredImageBatches = {};
	unordered_map<wstring, UIBatchDebugAggregate> uiBatchAggregates = {};
	UIImageBatchKey batchKey = {};
	_bool hasBatchKey = false;

	auto renderSingleImage = [&](CImage* img)
	{
		if (!img)
			return;

		BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);
		img->Bind_UIMaterial();
		img->Bind_Matrix();
		img->Bind_Camera(inverseMat, projMat);
		img->Bind_Mesh();
	};

	auto renderBatch = [&](const vector<CImage*>& batch)
	{
		if (batch.empty())
			return;

		for (size_t offset = 0; offset < batch.size(); offset += 128u)
		{
			vector<CImage*> chunk = {};
			chunk.reserve(min<size_t>(batch.size() - offset, 128u));

			for (size_t i = offset; i < batch.size() && chunk.size() < 128u; ++i)
				chunk.push_back(batch[i]);

			if (chunk.empty())
				continue;

			m_sRenderStats.uiImageBatches += 1u;
			m_sRenderStats.uiImageInstances += static_cast<_uint>(chunk.size());
			RecordUIBatchDebugAggregate(uiBatchAggregates, chunk);

			if (chunk.size() == 1u)
			{
				m_sRenderStats.uiImageSingleBatches += 1u;
				renderSingleImage(chunk.front());
				continue;
			}

			CImage* leader = chunk.front();
			BindUIInstanceBuffer(context, m_pUIInstanceBuffer, chunk);
			leader->Bind_UIMaterial();
			leader->Bind_Matrix();
			leader->Bind_Camera(inverseMat, projMat);
			if (CMeshBuffer* mesh = leader->Get_Mesh())
				mesh->Render_Instanced(static_cast<_uint>(chunk.size()));
			BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);
		}
	};

	auto flushImageBatch = [&]()
	{
		renderBatch(imageBatch);
		imageBatch.clear();
		hasBatchKey = false;

		if (!deferredImages.empty())
		{
			UIImageBatchKey dKey = {};
			_bool hasDKey = false;
			vector<CImage*> dBatch;

			for (CImage* dImg : deferredImages)
			{
				UIImageBatchKey nextDKey = BuildUIImageBatchKey(dImg);
				if (!hasDKey || !(dKey == nextDKey) || dBatch.size() >= 128)
				{
					renderBatch(dBatch);
					dBatch.clear();
					dKey = nextDKey;
					hasDKey = true;
				}
				dBatch.push_back(dImg);
			}
			renderBatch(dBatch);
			dBatch.clear();
			deferredImages.clear();
		}
	};

	auto flushLayeredImageBatches = [&]()
	{
		if (layeredImageBatches.empty())
			return;

		flushImageBatch();

		for (auto& layerEntryPair : layeredImageBatches)
		{
			vector<UILayeredBatchEntry>& entries = layerEntryPair.second;
			for (UILayeredBatchEntry& entry : entries)
				renderBatch(entry.images);
		}

		layeredImageBatches.clear();
	};

	auto renderGroupedImageBatches = [&]()
	{
		for (auto& groupedEntryPair : groupedImageBatches)
		{
			vector<UIGroupedBatchEntry>& entries = groupedEntryPair.second;
			for (UIGroupedBatchEntry& entry : entries)
				renderBatch(entry.images);
		}

		groupedImageBatches.clear();
	};

	for (TRAVERSAL_ITER(m_vUIList, it))
	{
		CUI* ui = *it;
		if (!ui || !ui->Get_GameObject()->IsRecursiveActive() || !ui->Get_Enable())
			continue;

		if (CImage* img = dynamic_cast<CImage*>(ui))
		{
			const _bool hasFill = img->Get_FillMethod() != CImage::FillMethod::None;

			if (hasFill && img->GetFillAmount() <= 0.f)
			{
				m_sRenderStats.uiImageZeroFillSkipped += 1u;
				continue;
			}

			if (hasFill && img->GetFillAmount() < 1.f)
				m_sRenderStats.uiImagePartialFillImages += 1u;

			if (HasUIImageBatchGroup(img))
			{
				flushLayeredImageBatches();
				flushImageBatch();
				QueueGroupedUIImage(groupedImageBatches, img);
				continue;
			}

			if (img->HasBatchLayer())
			{
				flushImageBatch();
				QueueLayeredUIImage(layeredImageBatches, img);
				continue;
			}

			if (!layeredImageBatches.empty())
				flushLayeredImageBatches();

			const UIImageBatchKey nextKey = BuildUIImageBatchKey(img);

			if (hasFill && img->GetFillAmount() >= 1.f
				&& hasBatchKey && !(batchKey == nextKey))
			{
				m_sRenderStats.uiImageDeferredFullFillImages += 1u;
				deferredImages.push_back(img);
				continue;
			}

			if (!hasBatchKey || !(batchKey == nextKey) || imageBatch.size() >= 128)
			{
				flushImageBatch();
				batchKey = nextKey;
				hasBatchKey = true;
			}

			imageBatch.push_back(img);
			continue;
		}

		if (!imageBatch.empty() || !deferredImages.empty() || !layeredImageBatches.empty())
			m_sRenderStats.uiTextFlushes += 1u;
		flushLayeredImageBatches();
		flushImageBatch();

		if (CText* txt = dynamic_cast<CText*>(ui))
			txt->RenderText();
	}

	flushLayeredImageBatches();
	flushImageBatch();
	renderGroupedImageBatches();
	FinalizeUIBatchDebugEntries(m_sRenderStats, uiBatchAggregates);

	m_vUIList.clear();
}

void CCamera::RenderUI_Editor()
{
	if (m_vUIList.empty())
		return;

	const _matrix viewMat = GetViewMatrix();
	const _matrix projMat = GetProjectionMatrix();
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	if (!context)
		return;

	BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);

	vector<CImage*> imageBatch = {};
	vector<CImage*> deferredImages = {};
	map<_int, vector<UIGroupedBatchEntry>> groupedImageBatches = {};
	map<_int, vector<UILayeredBatchEntry>> layeredImageBatches = {};
	unordered_map<wstring, UIBatchDebugAggregate> uiBatchAggregates = {};
	UIImageBatchKey batchKey = {};
	_bool hasBatchKey = false;

	auto renderSingleImage = [&](CImage* img)
	{
		if (!img)
			return;

		BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);
		img->Bind_UIMaterial();
		img->Bind_Matrix();
		img->Bind_Camera(viewMat, projMat);
		img->Bind_Mesh();
	};

	auto renderBatch = [&](const vector<CImage*>& batch)
	{
		if (batch.empty())
			return;

		for (size_t offset = 0; offset < batch.size(); offset += 128u)
		{
			vector<CImage*> chunk = {};
			chunk.reserve(min<size_t>(batch.size() - offset, 128u));

			for (size_t i = offset; i < batch.size() && chunk.size() < 128u; ++i)
				chunk.push_back(batch[i]);

			if (chunk.empty())
				continue;

			m_sRenderStats.uiImageBatches += 1u;
			m_sRenderStats.uiImageInstances += static_cast<_uint>(chunk.size());
			RecordUIBatchDebugAggregate(uiBatchAggregates, chunk);

			if (chunk.size() == 1u)
			{
				m_sRenderStats.uiImageSingleBatches += 1u;
				renderSingleImage(chunk.front());
				continue;
			}

			CImage* leader = chunk.front();
			BindUIInstanceBuffer(context, m_pUIInstanceBuffer, chunk);
			leader->Bind_UIMaterial();
			leader->Bind_Matrix();
			leader->Bind_Camera(viewMat, projMat);
			if (CMeshBuffer* mesh = leader->Get_Mesh())
				mesh->Render_Instanced(static_cast<_uint>(chunk.size()));
			BindUIInstanceBufferEmpty(context, m_pUIInstanceBuffer);
		}
	};

	auto flushImageBatch = [&]()
	{
		renderBatch(imageBatch);
		imageBatch.clear();
		hasBatchKey = false;

		if (!deferredImages.empty())
		{
			UIImageBatchKey dKey = {};
			_bool hasDKey = false;
			vector<CImage*> dBatch;

			for (CImage* dImg : deferredImages)
			{
				UIImageBatchKey nextDKey = BuildUIImageBatchKey(dImg);
				if (!hasDKey || !(dKey == nextDKey) || dBatch.size() >= 128)
				{
					renderBatch(dBatch);
					dBatch.clear();
					dKey = nextDKey;
					hasDKey = true;
				}
				dBatch.push_back(dImg);
			}
			renderBatch(dBatch);
			dBatch.clear();
			deferredImages.clear();
		}
	};

	auto flushLayeredImageBatches = [&]()
	{
		if (layeredImageBatches.empty())
			return;

		flushImageBatch();

		for (auto& layerEntryPair : layeredImageBatches)
		{
			vector<UILayeredBatchEntry>& entries = layerEntryPair.second;
			for (UILayeredBatchEntry& entry : entries)
				renderBatch(entry.images);
		}

		layeredImageBatches.clear();
	};

	auto renderGroupedImageBatches = [&]()
	{
		for (auto& groupedEntryPair : groupedImageBatches)
		{
			vector<UIGroupedBatchEntry>& entries = groupedEntryPair.second;
			for (UIGroupedBatchEntry& entry : entries)
				renderBatch(entry.images);
		}

		groupedImageBatches.clear();
	};

	for (TRAVERSAL_ITER(m_vUIList, it))
	{
		CUI* ui = *it;
		if (!ui || !ui->Get_GameObject()->IsRecursiveActive() || !ui->Get_Enable())
			continue;

		if (CImage* img = dynamic_cast<CImage*>(ui))
		{
			const _bool hasFill = img->Get_FillMethod() != CImage::FillMethod::None;

			if (hasFill && img->GetFillAmount() <= 0.f)
			{
				m_sRenderStats.uiImageZeroFillSkipped += 1u;
				continue;
			}

			if (hasFill && img->GetFillAmount() < 1.f)
				m_sRenderStats.uiImagePartialFillImages += 1u;

			if (HasUIImageBatchGroup(img))
			{
				flushLayeredImageBatches();
				flushImageBatch();
				QueueGroupedUIImage(groupedImageBatches, img);
				continue;
			}

			if (img->HasBatchLayer())
			{
				flushImageBatch();
				QueueLayeredUIImage(layeredImageBatches, img);
				continue;
			}

			if (!layeredImageBatches.empty())
				flushLayeredImageBatches();

			const UIImageBatchKey nextKey = BuildUIImageBatchKey(img);

			if (hasFill && img->GetFillAmount() >= 1.f
				&& hasBatchKey && !(batchKey == nextKey))
			{
				m_sRenderStats.uiImageDeferredFullFillImages += 1u;
				deferredImages.push_back(img);
				continue;
			}

			if (!hasBatchKey || !(batchKey == nextKey) || imageBatch.size() >= 128)
			{
				flushImageBatch();
				batchKey = nextKey;
				hasBatchKey = true;
			}

			imageBatch.push_back(img);
		}
		else
		{
			if (!imageBatch.empty() || !deferredImages.empty() || !layeredImageBatches.empty())
				m_sRenderStats.uiTextFlushes += 1u;
			flushLayeredImageBatches();
			flushImageBatch();

			if (CText* txt = dynamic_cast<CText*>(ui))
				txt->RenderText_Editor();
		}
	}

	flushLayeredImageBatches();
	flushImageBatch();
	renderGroupedImageBatches();
	FinalizeUIBatchDebugEntries(m_sRenderStats, uiBatchAggregates);

	m_vUIList.clear();
}

void CCamera::RenderDisplay()
{
	if (!m_pRectBuffer)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvCombine = rtm.GetSRV(CRenderTarget::RTType::Combine, m_bIsEditor);

	if (!srvCombine)
		return;

	CMaterial* presentMat = nullptr;
	{
		auto it = m_mRTDebugDisplays.find(CRenderTarget::RTType::Combine);
		if (it != m_mRTDebugDisplays.end())
			presentMat = it->second.material;
	}
	if (!presentMat)
		return;

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr; _float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	const D3D11_VIEWPORT* useVP = ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();
	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	const _float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = {};

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	presentMat->Bind_Matrix(w);
	presentMat->Bind_Camera(camPos, v, p, 0);

	ctx->PSSetShaderResources(0, 1, &srvCombine);
	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderRTDebugDisplay(const _bool _renderingEditorPass)
{
	if (m_bIsEditor)
	{
		if (!_renderingEditorPass)
			return;
	}
	else
	{
		if (_renderingEditorPass)
			return;

		if (!CEditor::GetInstance().IsSelected(m_pGameObject))
			return;
	}

	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();
	if (!context || m_mRTDebugDisplays.empty())
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	struct VisibleDebugDisplay
	{
		CRenderTarget::RTType type;
		_bool useEditorRT;
		ID3D11ShaderResourceView* srv;
		_float srcWidth;
		_float srcHeight;
	};

	vector<VisibleDebugDisplay> visibleDisplays;
	visibleDisplays.reserve(m_mRTDebugDisplays.size());

	const CRenderTarget::RTType candidateTypes[] =
	{
		CRenderTarget::RTType::Albedo,
		CRenderTarget::RTType::Object,
		CRenderTarget::RTType::Normal,
		CRenderTarget::RTType::Material,
		CRenderTarget::RTType::Depth,
		CRenderTarget::RTType::ShadowDepth,
		CRenderTarget::RTType::Diffuse,
		CRenderTarget::RTType::Specular,
		CRenderTarget::RTType::ShadowMask
	};

	for (CRenderTarget::RTType type : candidateTypes)
	{
		auto it = m_mRTDebugDisplays.find(type);
		if (it == m_mRTDebugDisplays.end())
			continue;

		const _bool useEditorRT = (type == CRenderTarget::RTType::ShadowDepth) ? false : m_bIsEditor;
		ID3D11ShaderResourceView* srv = rtm.GetSRV(type, useEditorRT);
		if (!srv)
			continue;

		_float srcWidth = static_cast<_float>(rtm.GetWidth(useEditorRT));
		_float srcHeight = static_cast<_float>(rtm.GetHeight(useEditorRT));

		if (ID3D11Texture2D* texture = rtm.GetTexture(type, useEditorRT))
		{
			D3D11_TEXTURE2D_DESC desc = {};
			texture->GetDesc(&desc);
			if (desc.Width > 0)
				srcWidth = static_cast<_float>(desc.Width);
			if (desc.Height > 0)
				srcHeight = static_cast<_float>(desc.Height);
		}

		visibleDisplays.push_back({ type, useEditorRT, srv, srcWidth, srcHeight });
	}

	if (visibleDisplays.empty())
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	context->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	context->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	context->RSGetState(&prevRS);
	context->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	context->RSGetViewports(&prevVPCount, &prevVP);

	if (m_pRTDebugDS)
		context->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		context->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	if (m_pRTDebugBS)
		context->OMSetBlendState(m_pRTDebugBS, bf, 0xFFFFFFFF);
	else
		context->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	vector2Int res = m_bIsEditor ? CEditor::GetInstance().Get_ScreenResolution() : CDisplay::GetInstance().Get_ScreenResolution();

	const D3D11_VIEWPORT* areaVP = ResolveViewport();
	if (!areaVP)
		areaVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	const _float areaX = areaVP ? areaVP->TopLeftX : 0.f;
	const _float areaY = areaVP ? areaVP->TopLeftY : 0.f;
	const _float areaW = areaVP ? areaVP->Width : (_float)res.x;
	const _float areaH = areaVP ? areaVP->Height : (_float)res.y;

	const _float screenW = max((_float)res.x, areaX + areaW);
	const _float screenH = max((_float)res.y, areaY + areaH);

	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = 0.f;
	vp.TopLeftY = 0.f;
	vp.Width = screenW;
	vp.Height = screenH;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	context->RSSetViewports(1, &vp);

	_matrix view = XMMatrixIdentity();
	_matrix proj = XMMatrixOrthographicOffCenterLH(0.f, screenW, screenH, 0.f, 0.f, 1.f);
	_float3 camPos = { 0.f, 0.f, -1.f };

	const _int kCount = static_cast<_int>(visibleDisplays.size());
	const _int kMaxPerColumn = 5;

	const _int rightRows = min(kMaxPerColumn, kCount);
	const _int leftRows = max(0, kCount - kMaxPerColumn);
	const _int maxRows = max(rightRows, leftRows);

	const _float margin = 12.f;
	const _float gap = 10.f;

	_float slotH = 0.f;
	if (maxRows > 0)
		slotH = (areaH - margin * 2.f - (_float)(maxRows - 1) * gap) / (_float)maxRows;

	if (slotH < 1.f)
		slotH = 1.f;

	const _float previewInset = 4.f;
	const _float debugRectH = max(slotH - previewInset * 2.f, 1.f);
	const _float baseY = areaY + areaH - margin - previewInset - debugRectH * 0.5f;

	for (_int i = 0; i < kCount; ++i)
	{
		const VisibleDebugDisplay& visible = visibleDisplays[i];
		auto it = m_mRTDebugDisplays.find(visible.type);
		if (it == m_mRTDebugDisplays.end())
			continue;

		RTDebugDisplay& disp = it->second;
		if (!disp.quad || !disp.material)
			continue;

		const _bool bRightColumn = (i < kMaxPerColumn);
		const _int row = bRightColumn ? i : (i - kMaxPerColumn);
		const _float srcW = visible.srcWidth;
		const _float srcH = visible.srcHeight;
		const _float srcAspect = (srcH > 0.f) ? (srcW / srcH) : 1.f;
		const _float debugRectW = debugRectH * srcAspect;

		_float cx = bRightColumn
			? (areaX + areaW - margin - debugRectW * 0.5f)
			: (areaX + margin + debugRectW * 0.5f);

		const _float cy = baseY - (_float)row * (slotH + gap);

		_matrix world = XMMatrixScaling(debugRectW, debugRectH, 1.f) * XMMatrixTranslation(cx, cy, 0.f);

		disp.material->Bind_Matrix(world);
		disp.material->Bind_Camera(camPos, view, proj, 0);

		context->PSSetShaderResources(0, 1, &visible.srv);
		disp.quad->Render();
	}

	CRenderTargetManager::GetInstance().Unbind_AllSRVs_PS(context, m_bIsEditor);

	if (prevVPCount > 0)
		context->RSSetViewports(1, &prevVP);

	context->OMSetRenderTargets(1, &prevRTV, prevDSV);

	context->OMSetDepthStencilState(prevDS, prevStencilRef);
	context->RSSetState(prevRS);
	context->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderObjectIDPass(const D3D11_VIEWPORT* vp)
{
	if (!m_bIsEditor || m_vUIList.empty())
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();
	ID3D11RenderTargetView* objectRTV = rtm.GetRTV(CRenderTarget::RTType::Object, m_bIsEditor);
	CMaterial* uiObjectIDMat = CResources::GetInstance().LoadOnGame<CMaterial>(L"UIObjectID (Material)");

	if (!objectRTV || !uiObjectIDMat)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();
	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	ctx->OMSetRenderTargets(1, &objectRTV, nullptr);

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float blendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, blendFactor, 0xFFFFFFFF);

	const _matrix viewMat = GetViewMatrix();
	const _matrix projMat = GetProjectionMatrix();

	for (TRAVERSAL_ITER(m_vUIList, it))
	{
		CUI* ui = *it;
		if (!ui || !ui->Get_GameObject())
			continue;
		if (!ui->Get_GameObject()->IsRecursiveActive() || !ui->Get_Enable())
			continue;
		if (ui->GetColor().a == 0)
			continue;

		uiObjectIDMat->Set_IntValue(L"gObjectID", ui->Get_GameObject()->Get_UniqueID());
		uiObjectIDMat->Bind_Matrix(ui->GetTransform()->Get_WorldMatrix());
		uiObjectIDMat->Bind_Camera(_float3(), viewMat, projMat);
		ui->Bind_Mesh();
	}

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderLightingCombined(const D3D11_VIEWPORT* vp)
{
	CMaterial* lightingMat = Find_RectMaterial(CRenderTarget::RTType::LightingCombined);

	if (!lightingMat || !m_pRectBuffer || !m_pInvViewProjCB || !m_pShadowCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvMaterial = rtm.GetSRV(CRenderTarget::RTType::Material, m_bIsEditor);
	ID3D11ShaderResourceView* srvShadowDepth = rtm.GetSRV(CRenderTarget::RTType::ShadowDepth, false);
	ID3D11RenderTargetView* rtvCombine = rtm.GetRTV(CRenderTarget::RTType::Combine, m_bIsEditor);

	if (!srvAlbedo || !srvNormal || !srvDepth || !srvMaterial || !rtvCombine)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);
	ctx->OMSetRenderTargets(1, &rtvCombine, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();
	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();
	if (useVP)
		ctx->RSSetViewports(1, useVP);

	if (m_eClearFlag == ClearFlags::Skybox || m_eClearFlag == ClearFlags::SolidColor)
	{
		const _float4 clearColor = m_vBackgroundColor.f4Color();
		const _float clear[4] = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
		ctx->ClearRenderTargetView(rtvCombine, clear);
	}

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	const _float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	const _float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = GetTransform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	ShadowCB scb = {};
	if (m_pMainLight && srvShadowDepth)
		PopulateShadowCB(this, m_pMainLight, m_sMainLightShadowData, scb);
	ctx->UpdateSubresource(m_pShadowCB, 0, nullptr, &scb, 0, 0);
	ctx->PSSetConstantBuffers(6, 1, &m_pShadowCB);

	lightingMat->Bind_Matrix(w);
	lightingMat->Bind_Camera(camPos, v, p, 0);

	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();
	lightingMat->Bind_Light(lights.empty() ? nullptr : lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[5] = { srvAlbedo, srvNormal, srvDepth, srvMaterial, srvShadowDepth };
	ctx->PSSetShaderResources(0, 5, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderLightingPass_ToDiffuse(const D3D11_VIEWPORT* vp)
{
	CMaterial* shadingMat = Find_RectMaterial(CRenderTarget::RTType::Diffuse);

	if (!shadingMat || !m_pRectBuffer || !m_pInvViewProjCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvMaterial = rtm.GetSRV(CRenderTarget::RTType::Material, m_bIsEditor);

	ID3D11RenderTargetView* rtvDiffuse = rtm.GetRTV(CRenderTarget::RTType::Diffuse, m_bIsEditor);

	if (!srvNormal || !srvDepth || !srvMaterial || !rtvDiffuse)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvDiffuse, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvDiffuse, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0.f;
	_float H = 0.f;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = GetTransform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	shadingMat->Bind_Matrix(w);
	shadingMat->Bind_Camera(camPos, v, p, 0);

	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();
	shadingMat->Bind_Light(lights.empty() ? nullptr : lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[4] = { srvAlbedo, srvNormal, srvDepth, srvMaterial };
	ctx->PSSetShaderResources(0, 4, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderLightingPass_ToSpecular(const D3D11_VIEWPORT* vp)
{
	CMaterial* specularMat = Find_RectMaterial(CRenderTarget::RTType::Specular);

	if (!specularMat || !m_pRectBuffer || !m_pInvViewProjCB)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvMaterial = rtm.GetSRV(CRenderTarget::RTType::Material, m_bIsEditor);
	ID3D11ShaderResourceView* srvEnv = CSceneManager::GetInstance().Get_CrtScene()->GetSkyBoxEnvironmentSRV();

	ID3D11RenderTargetView* rtvSpecular = rtm.GetRTV(CRenderTarget::RTType::Specular, m_bIsEditor);

	if (!srvAlbedo || !srvNormal || !srvDepth || !srvMaterial || !rtvSpecular)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvSpecular, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 0.f, 0.f, 0.f, 1.f };
	ctx->ClearRenderTargetView(rtvSpecular, clear);

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);
	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0;
	_float H = 0;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	_float3 camPos = GetTransform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	specularMat->Bind_Matrix(w);
	specularMat->Bind_Camera(camPos, v, p, 0);

	vector<_matrix>& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightData();
	specularMat->Bind_Light(lights.empty() ? nullptr : lights.data(), (_uint)lights.size());

	ID3D11ShaderResourceView* srvs[5] = { srvAlbedo, srvNormal, srvDepth, srvMaterial, srvEnv };
	ctx->PSSetShaderResources(0, 5, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderShadowDepthPass(const D3D11_VIEWPORT* vp)
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!device || !ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	if (!m_pMainLight)
		return;

	const _uint cascadeCount = min<_uint>(m_sMainLightShadowData.cascadeCount, kMaxShadowCascades);
	if (cascadeCount == 0u)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	if (m_pRTShadowDepthDS)
		ctx->OMSetDepthStencilState(m_pRTShadowDepthDS, 0);
	if (m_pRTShdowDepthRS)
		ctx->RSSetState(m_pRTShdowDepthRS);

	const _float bf[4] = { 0,0,0,0 };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	CMaterial* shadowDepthMat = Find_RectMaterial(CRenderTarget::RTType::ShadowDepth);

	auto isRenderableShadowTarget = [](CRenderer* r)
	{
		if (!r || !r->Get_GameObject())
			return false;
		if (!r->Get_GameObject()->IsRecursiveActive())
			return false;
		if (!r->Get_Enable())
			return false;
		if (!r->IsCastShadow())
			return false;
		if (!r->Get_MeshBuffer())
			return false;
		return true;
	};

	auto isShadowVisible = [this](CRenderer* r, const CLight::ShadowCascade& cascade)
	{
		BoundingBox worldAABB = {};
		if (!TryBuildRendererWorldAABB(r, worldAABB))
			return false;

		const _matrix lightView = XMLoadFloat4x4(&cascade.matrices.view);
		XMFLOAT3 corners[8] = {};
		worldAABB.GetCorners(corners);

		_vector minV = XMVectorSet(FLT_MAX, FLT_MAX, FLT_MAX, 0.f);
		_vector maxV = XMVectorSet(-FLT_MAX, -FLT_MAX, -FLT_MAX, 0.f);
		for (_int i = 0; i < 8; ++i)
		{
			_vector p = XMLoadFloat3(&corners[i]);
			_vector pLS = XMVector3TransformCoord(p, lightView);
			minV = XMVectorMin(minV, pLS);
			maxV = XMVectorMax(maxV, pLS);
		}

		_float3 aabbMin = {};
		_float3 aabbMax = {};
		XMStoreFloat3(&aabbMin, minV);
		XMStoreFloat3(&aabbMax, maxV);

		if (aabbMax.x < cascade.lightSpaceMin.x || aabbMin.x > cascade.lightSpaceMax.x)
			return false;
		if (aabbMax.y < cascade.lightSpaceMin.y || aabbMin.y > cascade.lightSpaceMax.y)
			return false;
		if (aabbMax.z < cascade.lightSpaceMin.z || aabbMin.z > cascade.lightSpaceMax.z)
			return false;

		return true;
	};

	struct ShadowBatchKey
	{
		CMeshBuffer* meshBuffer;
		CMaterial* material;
		_bool castShadow;
		_bool isMirrored;

		_bool operator==(const ShadowBatchKey& rhs) const
		{
			return meshBuffer == rhs.meshBuffer
				&& material == rhs.material
				&& castShadow == rhs.castShadow
				&& isMirrored == rhs.isMirrored;
		}
	};

	struct ShadowBatchKeyHash
	{
		size_t operator()(const ShadowBatchKey& key) const
		{
			size_t h1 = hash<void*>()(static_cast<void*>(key.meshBuffer));
			size_t h2 = hash<void*>()(static_cast<void*>(key.material));
			size_t h3 = hash<int>()(static_cast<int>(key.castShadow));
			size_t h4 = hash<int>()(static_cast<int>(key.isMirrored));
			return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3);
		}
	};

	const _uint shadowSize = (_uint)CSceneManager::GetInstance().Get_LightSetting().shadowMapSize;
	D3D11_VIEWPORT vpt = {};
	vpt.TopLeftX = 0.f;
	vpt.TopLeftY = 0.f;
	vpt.Width = (_float)shadowSize;
	vpt.Height = (_float)shadowSize;
	vpt.MinDepth = 0.f;
	vpt.MaxDepth = 1.f;
	ctx->RSSetViewports(1, &vpt);

	for (_uint cascadeIndex = 0u; cascadeIndex < cascadeCount; ++cascadeIndex)
	{
		ID3D11DepthStencilView* dsvShadow = rtm.GetDSV(CRenderTarget::RTType::ShadowDepth, false, cascadeIndex);
		if (!dsvShadow)
			continue;

		ctx->OMSetRenderTargets(0, nullptr, dsvShadow);
		ctx->ClearDepthStencilView(dsvShadow, D3D11_CLEAR_DEPTH, 1.0f, 0);

		const CLight::ShadowCascade& cascade = m_sMainLightShadowData.cascades[cascadeIndex];

		unordered_map<ShadowBatchKey, vector<CRenderer*>, ShadowBatchKeyHash> staticBatches;
		staticBatches.reserve(m_vStaticMeshList.size());

		for (auto* r : m_vStaticMeshList)
		{
			if (!isRenderableShadowTarget(r))
				continue;
			if (!isShadowVisible(r, cascade))
				continue;

			ShadowBatchKey key = { r->Get_MeshBuffer(), r->Get_Material(), r->IsCastShadow(), r->IsMirroredTransform() };
			staticBatches[key].push_back(r);
		}

		for (auto& kv : staticBatches)
		{
			auto& batch = kv.second;
			if (batch.empty())
				continue;

			CRenderer* leader = batch[0];
			if (!leader || !leader->Get_GameObject() || !leader->GetTransform())
				continue;

			const _uint maxInstanceCount = 128u;

			for (size_t offset = 0; offset < batch.size(); offset += maxInstanceCount)
			{
				const size_t remain = batch.size() - offset;
				const _uint chunkCount = static_cast<_uint>(min<size_t>(remain, maxInstanceCount));

				leader->CreateMeshInstancing(chunkCount);

				for (_uint i = 0; i < chunkCount; ++i)
				{
					CRenderer* r = batch[offset + i];
					if (!r || !r->GetTransform())
						continue;

					leader->SetInstancingWorldMatrix(i, r->GetTransform()->GetSnapshotWorldMatrix());
				}

				leader->Render_ShadowDepth(shadowDepthMat, cascade.matrices);
				leader->CreateMeshInstancing(0);
			}
		}

		for (auto& entry : m_vDynamicMeshEntries)
		{
			auto* r = entry.renderer;
			if (!isRenderableShadowTarget(r))
				continue;
			if (!isShadowVisible(r, cascade))
				continue;

			r->Render_ShadowDepth(shadowDepthMat, cascade.matrices);
		}
	}

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0) ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderShadowMaskPass(const D3D11_VIEWPORT* vp)
{
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();

	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11RenderTargetView* rtvShadowMask = rtm.GetRTV(CRenderTarget::RTType::ShadowMask, m_bIsEditor);
	if (!rtvShadowMask)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP{};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);
	ctx->OMSetRenderTargets(1, &rtvShadowMask, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	const _float clear[4] = { 1.f, 1.f, 1.f, 1.f };
	ctx->ClearRenderTargetView(rtvShadowMask, clear);

	if (!m_pRectBuffer || !m_pInvViewProjCB || !m_pShadowCB)
	{
		ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
		if (prevVPCount > 0)
			ctx->RSSetViewports(1, &prevVP);

		ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
		ctx->RSSetState(prevRS);
		ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

		Safe_Release(prevRTV);
		Safe_Release(prevDSV);
		Safe_Release(prevDS);
		Safe_Release(prevRS);
		Safe_Release(prevBS);
		return;
	}

	ID3D11ShaderResourceView* srvSceneDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvSceneNormal = rtm.GetSRV(CRenderTarget::RTType::Normal, m_bIsEditor);
	ID3D11ShaderResourceView* srvShadowDepth = rtm.GetSRV(CRenderTarget::RTType::ShadowDepth, false);
	CMaterial* shadowMaskMat = Find_RectMaterial(CRenderTarget::RTType::ShadowMask);

	if (!srvSceneDepth || !srvSceneNormal || !srvShadowDepth || !shadowMaskMat || !m_pMainLight)
	{
		ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
		if (prevVPCount > 0)
			ctx->RSSetViewports(1, &prevVP);

		ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
		ctx->RSSetState(prevRS);
		ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

		Safe_Release(prevRTV);
		Safe_Release(prevDSV);
		Safe_Release(prevDS);
		Safe_Release(prevRS);
		Safe_Release(prevBS);
		return;
	}

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = 0;
	_float H = 0;

	if (!m_bIsEditor)
	{
		W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;
	}
	else
	{
		W = useVP ? useVP->Width : (_float)CEditor::GetInstance().Get_ScreenResolution().x;
		H = useVP ? useVP->Height : (_float)CEditor::GetInstance().Get_ScreenResolution().y;
	}

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);
	_float3 camPos = GetTransform()->Get_Position();

	InvViewProjCB invCB = { m_vVPInverseMatrix };
	ctx->UpdateSubresource(m_pInvViewProjCB, 0, nullptr, &invCB, 0, 0);
	ctx->PSSetConstantBuffers(5, 1, &m_pInvViewProjCB);

	ShadowCB scb = {};
	PopulateShadowCB(this, m_pMainLight, m_sMainLightShadowData, scb);
	ctx->UpdateSubresource(m_pShadowCB, 0, nullptr, &scb, 0, 0);
	ctx->PSSetConstantBuffers(6, 1, &m_pShadowCB);

	shadowMaskMat->Bind_Matrix(w);
	shadowMaskMat->Bind_Camera(camPos, v, p, 0);

	ID3D11ShaderResourceView* srvs[3] = { srvSceneDepth, srvSceneNormal, srvShadowDepth };
	ctx->PSSetShaderResources(0, 3, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

void CCamera::RenderCombine(const D3D11_VIEWPORT* vp)
{
	if (!m_pRectBuffer)
		return;

	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!ctx)
		return;

	auto& rtm = CRenderTargetManager::GetInstance();

	ID3D11ShaderResourceView* srvAlbedo = rtm.GetSRV(CRenderTarget::RTType::Albedo, m_bIsEditor);
	ID3D11ShaderResourceView* srvDepth = rtm.GetSRV(CRenderTarget::RTType::Depth, m_bIsEditor);
	ID3D11ShaderResourceView* srvDiffuse = rtm.GetSRV(CRenderTarget::RTType::Diffuse, m_bIsEditor);
	ID3D11ShaderResourceView* srvSpecular = rtm.GetSRV(CRenderTarget::RTType::Specular, m_bIsEditor);
	ID3D11ShaderResourceView* srvShadow = rtm.GetSRV(CRenderTarget::RTType::ShadowMask, m_bIsEditor);

	ID3D11RenderTargetView* rtvCombine = rtm.GetRTV(CRenderTarget::RTType::Combine, m_bIsEditor);

	if (!srvDepth || !srvAlbedo || !srvDiffuse || !srvSpecular || !srvShadow || !rtvCombine)
		return;

	ID3D11RenderTargetView* prevRTV = nullptr;
	ID3D11DepthStencilView* prevDSV = nullptr;
	ctx->OMGetRenderTargets(1, &prevRTV, &prevDSV);

	D3D11_VIEWPORT prevVP = {};
	_uint prevVPCount = 1;
	ctx->RSGetViewports(&prevVPCount, &prevVP);

	ID3D11DepthStencilState* prevDS = nullptr;
	_uint prevStencilRef = 0;
	ID3D11RasterizerState* prevRS = nullptr;
	ID3D11BlendState* prevBS = nullptr;
	_float prevBlendFactor[4] = {};
	_uint  prevSampleMask = 0;

	ctx->OMGetDepthStencilState(&prevDS, &prevStencilRef);
	ctx->RSGetState(&prevRS);
	ctx->OMGetBlendState(&prevBS, prevBlendFactor, &prevSampleMask);

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &rtvCombine, nullptr);

	const D3D11_VIEWPORT* useVP = vp ? vp : ResolveViewport();

	if (!useVP)
		useVP = CGraphicDevice::GetInstance().Get_CurrentViewport();

	if (useVP)
		ctx->RSSetViewports(1, useVP);

	if (m_eClearFlag == ClearFlags::Skybox || m_eClearFlag == ClearFlags::SolidColor)
	{
		const _float4 clearColor = m_vBackgroundColor.f4Color();
		const _float clear[4] = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
		ctx->ClearRenderTargetView(rtvCombine, clear);
	}

	if (m_pRTDebugDS)
		ctx->OMSetDepthStencilState(m_pRTDebugDS, 0);
	if (m_pRTDebugRS)
		ctx->RSSetState(m_pRTDebugRS);

	const _float bf[4] = { 0.f, 0.f, 0.f, 0.f };
	ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

	_float W = useVP ? useVP->Width : (_float)CDisplay::GetInstance().Get_ScreenResolution().x;
	_float H = useVP ? useVP->Height : (_float)CDisplay::GetInstance().Get_ScreenResolution().y;

	_matrix v = XMMatrixIdentity();
	_matrix p = XMMatrixOrthographicOffCenterLH(0.f, W, H, 0.f, 0.f, 1.f);
	_matrix w = XMMatrixScaling(W, H, 1.f) * XMMatrixTranslation(W * 0.5f, H * 0.5f, 0.f);

	_float3 camPos = {};

	CMaterial* combineMat = Find_RectMaterial(CRenderTarget::RTType::Combine);

	combineMat->Bind_Matrix(w);
	combineMat->Bind_Camera(camPos, v, p, 0);

	ID3D11ShaderResourceView* srvs[5] = { srvAlbedo, srvDepth, srvDiffuse, srvSpecular, srvShadow };
	ctx->PSSetShaderResources(0, 5, srvs);

	m_pRectBuffer->Render();

	rtm.Unbind_AllSRVs_PS(ctx, m_bIsEditor);

	ctx->OMSetRenderTargets(1, &prevRTV, prevDSV);
	if (prevVPCount > 0)
		ctx->RSSetViewports(1, &prevVP);

	ctx->OMSetDepthStencilState(prevDS, prevStencilRef);
	ctx->RSSetState(prevRS);
	ctx->OMSetBlendState(prevBS, prevBlendFactor, prevSampleMask);

	Safe_Release(prevRTV);
	Safe_Release(prevDSV);
	Safe_Release(prevDS);
	Safe_Release(prevRS);
	Safe_Release(prevBS);
}

const _int CCamera::GetColorPickingID(const vector2Int& _mouseVPPos)
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	auto& rtm = CRenderTargetManager::GetInstance();
	ID3D11DeviceContext* ctx = CGraphicDevice::GetInstance().Get_Context();
	if (!device || !ctx)
		return 0;

	if (!EnsurePickStaging())
		return 0;

	ID3D11Texture2D* srcTex = rtm.GetTexture(CRenderTarget::RTType::Object, m_bIsEditor);
	if (!srcTex) return 0;

	D3D11_BOX box;
	box.left = _mouseVPPos.x;
	box.right = _mouseVPPos.x + 1;
	box.top = _mouseVPPos.y;
	box.bottom = _mouseVPPos.y + 1;
	box.front = 0;
	box.back = 1;

	ctx->CopySubresourceRegion
	(
		m_pPickStaging, 0,
		0, 0, 0,
		srcTex, 0,
		&box);

	D3D11_MAPPED_SUBRESOURCE mapped = {};
	if (FAILED(ctx->Map(m_pPickStaging, 0, D3D11_MAP_READ, 0, &mapped)))
		return 0;

	const uint8_t* p = (const uint8_t*)mapped.pData;
	uint32_t r = p[0];
	uint32_t g = p[1];
	uint32_t b = p[2];

	ctx->Unmap(m_pPickStaging, 0);

	uint32_t id = (r) | (g << 8) | (b << 16);

	return id;
}

CPhysics::Ray CCamera::ScreenPointToRay(const vector2Int& _pixel, _float _maxDist)
{
	return CPhysics::Ray();
}

CPhysics::Ray CCamera::ScreenPointToRay_Editor(const vector2Int& _pixel, _float _maxDist)
{
	auto eo = CEditor::GetInstance().Get_Options();

	auto res = CEditor::GetInstance().Get_ScreenResolution();
	_float w = static_cast<_float>(res.x - 40);
	_float h = static_cast<_float>(res.y + 20);

	_float xNdc = 2.0f * _pixel.x / w - 1.0f;
	_float yNdc = -2.0f * _pixel.y / h + 1.0f;

	_vector ptNear = XMVectorSet(xNdc, yNdc, 0.f, 1.f);
	_vector ptFar = XMVectorSet(xNdc, yNdc, 1.f, 1.f);

	_matrix view = XMLoadFloat4x4(&m_vViewMatrix);
	_matrix proj = XMLoadFloat4x4(&m_vProjMatrix);
	_matrix invVP = XMMatrixInverse(nullptr, view * proj);

	ptNear = XMVector4Transform(ptNear, invVP);
	ptFar = XMVector4Transform(ptFar, invVP);
	ptNear /= XMVectorGetW(ptNear);
	ptFar /= XMVectorGetW(ptFar);

	_float3 origin, dir;
	XMStoreFloat3(&origin, ptNear);

	if (m_eCamViewMode == ViewMode::Perspective)
	{
		_vector camPos = GetTransform()->Get_WorldMatrix().r[3];
		_vector rayDir = XMVectorSubtract(ptFar, camPos);

		XMStoreFloat3(&origin, camPos);
		XMStoreFloat3(&dir, rayDir);
	}
	else
	{
		XMStoreFloat3(&origin, ptNear);
		dir = GetTransform()->Get_Directions().forward;
	}

	vector3 resultDir = vector3(dir);

	CPhysics::Ray result = { origin, resultDir.normalized(), _maxDist };
	return result;
}

const _bool CCamera::EnsurePickStaging()
{
	if (m_pPickStaging)
		return true;

	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();

	if (!device)
		return false;

	D3D11_TEXTURE2D_DESC d = {};
	d.Width = 1;
	d.Height = 1;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_R32_UINT;
	d.SampleDesc.Count = 1;
	d.SampleDesc.Quality = 0;
	d.Usage = D3D11_USAGE_STAGING;
	d.BindFlags = 0;
	d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	d.MiscFlags = 0;

	return SUCCEEDED(device->CreateTexture2D(&d, nullptr, &m_pPickStaging));
}

CMaterial* CCamera::Add_RectMaterial(const CRenderTarget::RTType _type, const wstring& _path)
{
	CMaterial* newMat = CResources::GetInstance().LoadOnGame<CMaterial>(_path);

	if (!newMat)
	{
		CDebug::LogError((wstring(L"Not found ") + _path));
		Safe_Release(m_pMainLight);
		return nullptr;
	}

	auto it = m_mRectMats.find(_type);

	if (it != m_mRectMats.end())
		Safe_Release(it->second);

	m_mRectMats[_type] = newMat;

	return newMat;
}

CMaterial* CCamera::Find_RectMaterial(const CRenderTarget::RTType _type)
{
	auto it = m_mRectMats.find(_type);

	if (it == m_mRectMats.end())
		return nullptr;

	return it->second;
}

const D3D11_VIEWPORT* CCamera::ResolveViewport() const
{
	return m_bIsEditor ? CGraphicDevice::GetInstance().Get_EditorViewport() : CGraphicDevice::GetInstance().Get_GameViewport();
}

void CCamera::Find_MainLight()
{
	auto& lights = CSceneManager::GetInstance().Get_CrtScene()->Get_LightList();

	if (lights.empty())
	{
		if (m_pMainLight)
		{
			Safe_Release(m_pMainLight);
			m_pMainLight = nullptr;
		}
		return;
	}

	if (!m_pMainLight)
	{
		for (TRAVERSAL_ITER(lights, it))
		{
			if ((*it)->Get_Enable() && (*it)->Get_GameObject()->IsActive() && (*it)->IsCastShadow() && (*it)->Get_Type() == CLight::Type::Directional)
			{
				m_pMainLight = *it;
				m_pMainLight->AddRef();
				break;
			}
		}
	}
	else
	{
		if (!m_pMainLight->Get_GameObject()->IsActive() || !m_pMainLight->Get_Enable())
		{
			Safe_Release(m_pMainLight);
			m_pMainLight = nullptr;
		}
	}
}







