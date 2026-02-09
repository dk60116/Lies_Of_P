#include "epch.h"
#include "Resources.h"

#include <shlobj.h> 
#include <shlwapi.h> 
#include <tchar.h>

#pragma comment(lib, "Shlwapi.lib")

CResources::CResources()
	: m_strDefaultAssetPath(L"../Assets/")
	, m_strEngineFilePath(L"../EngineResources/")
	, m_mEditorResourceList({})
	, m_mGameResourceList({})
{
}

CResources::~CResources()
{
	Release();
}

CResources& CResources::GetInstance()
{
	static CResources inst;
	return inst;
}

HRESULT CResources::Initialize()
{
	if (!fs::exists("BinaryAssets"))
		fs::create_directories("BinaryAssets");
	if (!fs::exists("BinaryAssets/SceneData"))
		fs::create_directories("BinaryAssets/SceneData");
	if (!fs::exists("BinaryAssets/MeshData"))
		fs::create_directories("BinaryAssets/MeshData");
	if (!fs::create_directory("BinaryAssets/SkinnedMeshData"))
		fs::create_directories("BinaryAssets/SkinnedMeshData");
	if (!fs::exists("BinaryAssets/AnimationClipData"))
		fs::create_directories("BinaryAssets/AnimationClipData");
	if (!fs::exists("BinaryAssets/FontData"))
		fs::create_directories("BinaryAssets/FontData");

	Ready_GameResources();

	return S_OK;
}

void CResources::Release()
{
	for (TRAVERSAL_ITER(m_mGameResourceList, it))
		Safe_Release((*it).second);

	m_mGameResourceList.clear();
}

HRESULT CResources::ConvertFBXToMeshBufferData(const wstring _filePath)
{
	Assimp::Importer importer;
	const aiScene* aiScene = importer.ReadFile
	(
		CEngineString::WStringToString(GetInstance().m_strDefaultAssetPath + _filePath),
		aiProcess_Triangulate |
		//aiProcess_JoinIdenticalVertices |
		//aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_ConvertToLeftHanded |
		aiProcess_FlipUVs
	);

	if (!aiScene || !aiScene->HasMeshes())
	{
		CDebug::LogError(L"Failed create mesh buffer - Invalid scene: " + _filePath);
		return E_FAIL;
	}

	const _bool hasMaterial = aiScene->HasMaterials();
	using VTX = VertexTexNormalTangentBuffer;

	vector<aiMatrix4x4> meshGlobalMats(aiScene->mNumMeshes, aiMatrix4x4());

	function<void(aiNode*, const aiMatrix4x4&)> BuildMeshTransforms =
		[&](aiNode* node, const aiMatrix4x4& parentTrafo)
		{
			aiMatrix4x4 current = parentTrafo * node->mTransformation;
			for (_uint mi = 0; mi < node->mNumMeshes; ++mi)
				meshGlobalMats[node->mMeshes[mi]] = current;

			for (_uint ci = 0; ci < node->mNumChildren; ++ci)
				BuildMeshTransforms(node->mChildren[ci], current);
		};
	BuildMeshTransforms(aiScene->mRootNode, aiMatrix4x4());

	vector<CMeshBuffer::MeshBufferInitiaizeInfo> bufferInfoList;

	for (_uint i = 0; i < aiScene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = aiScene->mMeshes[i];
		const aiMatrix4x4& gMat = meshGlobalMats[i];
		aiMatrix3x3 gMat3 = aiMatrix3x3(gMat).Inverse().Transpose();

		CMeshBuffer::MeshBufferInitiaizeInfo info{};
		info.meshName = CMeshBuffer::FindMeshName(aiScene, i);

		vector<VTX>   vertices;
		vector<_uint> indices;

		vertices.reserve(mesh->mNumVertices);
		for (_uint v = 0; v < mesh->mNumVertices; ++v)
		{
			aiVector3D pos = gMat * mesh->mVertices[v];

			aiVector3D nor(0, 0, 0), tan(0, 0, 0);
			if (mesh->HasNormals())
				nor = gMat3 * mesh->mNormals[v];
			if (mesh->HasTangentsAndBitangents())
				tan = gMat3 * mesh->mTangents[v];

			VTX vert{};
			vert.position = { pos.x, pos.y, pos.z };
			vert.normal = { nor.x, nor.y, nor.z };
			vert.tangent = { tan.x, tan.y, tan.z };
			vert.uv = mesh->HasTextureCoords(0) ? _float2{ mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y } : _float2{ 0, 0 };

			vertices.emplace_back(vert);
		}

		for (_uint f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices == 3)
			{
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
		}

		CMeshBuffer::MESHBUFFERDESC desc{};
		desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.vertexSize = sizeof(VTX);
		desc.vertextCount = static_cast<_uint>(vertices.size());
		desc.indexCount = static_cast<_uint>(indices.size());

		info.buffer.assign(reinterpret_cast<const uint8_t*>(vertices.data()),
			reinterpret_cast<const uint8_t*>(vertices.data()) +
			vertices.size() * sizeof(VTX));
		info.indices.assign(indices.begin(), indices.end());
		info.desc = desc;

		if (hasMaterial)
		{
			aiMaterial* mat = aiScene->mMaterials[mesh->mMaterialIndex];
			aiString texPath;

			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS)
			{
				filesystem::path fbxDir = filesystem::path(_filePath).parent_path();
				filesystem::path relPath = filesystem::u8path(texPath.C_Str());
				filesystem::path fullPath = fbxDir / relPath;
				info.diffuseMapPath = GetInstance().m_strDefaultAssetPath + fullPath.wstring();
			}
		}

		bufferInfoList.push_back(move(info));
	}

	auto split = CEngineString::Split(_filePath, L"/");
	wstring folder = split[split.size() - 2];
	wstring fileNoExt = CEngineString::Split(split.back(), L".")[0];
	wstring saveName = folder + L"_" + fileNoExt;

	if (FAILED(SaveMeshBufferInfos(
		L"BinaryAssets/MeshData/" + saveName + L".meshdata",
		bufferInfoList)))
	{
		CDebug::LogError(L"Failed create mesh Data - can not save: " + _filePath);
		return E_FAIL;
	}

	CDebug::Log(L"Complete create mesh Data: " + _filePath);

	return S_OK;
}

HRESULT CResources::ConvertFBXToSkinnedBufferData(const wstring _filePath)
{
	Assimp::Importer importer;
	const aiScene* aiScene = importer.ReadFile
	(
		CEngineString::WStringToString(m_strDefaultAssetPath + _filePath),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_ConvertToLeftHanded |
		aiProcess_FlipUVs
	);

	if (!aiScene || !aiScene->HasMeshes())
	{
		CDebug::LogError(L"Failed create skinned buffer - Invalid scene: " + _filePath);
		return E_FAIL;
	}

	bool hasBones = false;
	for (_uint i = 0; i < aiScene->mNumMeshes; ++i)
		hasBones |= aiScene->mMeshes[i]->HasBones();

	if (!hasBones)
	{
		CDebug::LogError(L"Failed create skinned buffer - No bones: " + _filePath);
		return E_FAIL;
	}

	const bool hasMaterial = aiScene->HasMaterials();
	using VTX = VertexSkinnedBuffer;

	vector<aiMatrix4x4> meshGlobalMats(aiScene->mNumMeshes, aiMatrix4x4());

	function<void(aiNode*, const aiMatrix4x4&)> BuildMeshTransforms =
		[&](aiNode* node, const aiMatrix4x4& parentTrafo)
		{
			aiMatrix4x4 current = parentTrafo * node->mTransformation;
			for (_uint mi = 0; mi < node->mNumMeshes; ++mi)
				meshGlobalMats[node->mMeshes[mi]] = current;

			for (_uint ci = 0; ci < node->mNumChildren; ++ci)
				BuildMeshTransforms(node->mChildren[ci], current);
		};
	BuildMeshTransforms(aiScene->mRootNode, aiMatrix4x4());

	vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> bufferInfoList;

	for (_uint i = 0; i < aiScene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = aiScene->mMeshes[i];
		const aiMatrix4x4& gMat = meshGlobalMats[i];
		aiMatrix3x3         gMat3 = aiMatrix3x3(gMat).Inverse().Transpose();

		CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo info{};
		info.meshName = CMeshBuffer::FindMeshName(aiScene, i);

		vector<VTX>   vertices;
		vector<_uint> indices;

		vertices.reserve(mesh->mNumVertices);
		for (_uint v = 0; v < mesh->mNumVertices; ++v)
		{
			aiVector3D pos = gMat * mesh->mVertices[v];

			aiVector3D nor(0, 0, 0), tan(0, 0, 0);
			if (mesh->HasNormals())              nor = gMat3 * mesh->mNormals[v];
			if (mesh->HasTangentsAndBitangents()) tan = gMat3 * mesh->mTangents[v];

			VTX vert{};
			vert.position = { pos.x, pos.y, pos.z };
			vert.normal = { nor.x, nor.y, nor.z };
			vert.tangent = { tan.x, tan.y, tan.z };
			vert.uv = mesh->HasTextureCoords(0) ? _float2{ mesh->mTextureCoords[0][v].x, mesh->mTextureCoords[0][v].y } : _float2{ 0, 0 };

			vertices.emplace_back(vert);
		}

		unordered_map<string, _uint> boneNameToIndex;
		_uint boneIdxCounter = 0;

		for (_uint b = 0; b < mesh->mNumBones; ++b)
		{
			aiBone* bone = mesh->mBones[b];
			string boneName = bone->mName.C_Str();

			_uint boneIdx = 0;
			auto it = boneNameToIndex.find(boneName);
			if (it == boneNameToIndex.end())
			{
				boneIdx = boneIdxCounter++;
				boneNameToIndex.insert({ boneName, boneIdx });
			}
			else boneIdx = it->second;

			for (_uint w = 0; w < bone->mNumWeights; ++w)
			{
				_uint vid = bone->mWeights[w].mVertexId;
				float     bw = bone->mWeights[w].mWeight;
				if (vid < vertices.size())
					CSkinnedMeshBuffer::FillBoneWeights(vertices[vid], boneIdx, bw);
			}
		}

		for (_uint f = 0; f < mesh->mNumFaces; ++f)
		{
			const aiFace& face = mesh->mFaces[f];
			if (face.mNumIndices == 3)  
			{
				indices.push_back(face.mIndices[0]);
				indices.push_back(face.mIndices[1]);
				indices.push_back(face.mIndices[2]);
			}
		}

		CMeshBuffer::MESHBUFFERDESC desc{};
		desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		desc.vertexSize = sizeof(VTX);
		desc.vertextCount = static_cast<_uint>(vertices.size());
		desc.indexCount = static_cast<_uint>(indices.size());

		info.buffer.assign(reinterpret_cast<const uint8_t*>(vertices.data()),
			reinterpret_cast<const uint8_t*>(vertices.data()) +
			vertices.size() * sizeof(VTX));
		info.indices.assign(indices.begin(), indices.end());
		info.desc = desc;

		if (hasMaterial)
		{
			aiMaterial* mat = aiScene->mMaterials[mesh->mMaterialIndex];
			aiString texPath;

			if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS)
			{
				filesystem::path fbxDir = filesystem::path(_filePath).parent_path();
				filesystem::path relPath = filesystem::u8path(texPath.C_Str());
				filesystem::path fullPath = fbxDir / relPath;
				info.diffuseMapPath = m_strDefaultAssetPath + fullPath.wstring();
			}
		}

		for (_uint b = 0; b < mesh->mNumBones; ++b)
		{
			aiBone* bone = mesh->mBones[b];
			info.boneNames.push_back(CEngineString::StringToWString(bone->mName.C_Str()));

			const aiMatrix4x4& m = bone->mOffsetMatrix;
			_float4x4 o = _float4x4(
				m.a1, m.b1, m.c1, m.d1,
				m.a2, m.b2, m.c2, m.d2,
				m.a3, m.b3, m.c3, m.d3,
				m.a4, m.b4, m.c4, m.d4);
			info.boneOffsetMatrices.push_back(o);
		}

		bufferInfoList.push_back(move(info));
	}

	vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> skeletalHierarchy;
	unordered_map<aiNode*, _uint> nodeToIdMap;
	TraverseSkeleton(aiScene->mRootNode, -1, skeletalHierarchy);

	auto split = CEngineString::Split(_filePath, L"/");
	wstring folder = split[split.size() - 2];
	wstring fileNoExt = CEngineString::Split(split.back(), L".")[0];
	wstring saveName = folder + L"_" + fileNoExt;

	if (FAILED(SaveSkinnedBufferInfos(
		L"BinaryAssets/SkinnedMeshData/" + saveName + L".skinneddata",
		bufferInfoList, skeletalHierarchy)))
	{
		CDebug::LogError(L"Failed create skinned mesh Data - can not save: " + _filePath);
		return E_FAIL;
	}

	CDebug::Log(L"Complete create skinned mesh Data: " + _filePath);
	return S_OK;
}

HRESULT CResources::ConvertFBXToAnimationClipData(const wstring _filePath)
{
	Assimp::Importer importer;
	const aiScene* aiScene = importer.ReadFile
	(
		CEngineString::WStringToString(m_strDefaultAssetPath + _filePath),
		aiProcess_Triangulate |
		aiProcess_JoinIdenticalVertices |
		aiProcess_GenNormals |
		aiProcess_CalcTangentSpace |
		aiProcess_ConvertToLeftHanded |
		aiProcess_FlipUVs
	);

	if (!aiScene)
	{
		CDebug::LogError(L"Failed create skinned buffer - Can not create AIScene: " + _filePath);
		return E_FAIL;
	}

	if (!aiScene->HasAnimations())
	{
		CDebug::LogError(L"Failed create animation clip buffer - AIScene has not animations: " + _filePath);
		return E_FAIL;
	}

	vector<CAnimationClip::AnimationClipInitInfo> animationInfoList = {};

	for (_uint i = 0; i < aiScene->mNumAnimations; i++)
	{
		CAnimationClip::AnimationClipInitInfo animInfo = {};

		const aiAnimation* anim = aiScene->mAnimations[i];

		animInfo.name = CEngineString::StringToWString(anim->mName.C_Str());
		animInfo.duration = static_cast<_float>(anim->mDuration);
		animInfo.ticksPerSecond = (anim->mTicksPerSecond != 0.0) ? static_cast<_float>(anim->mTicksPerSecond) : 30.f;

		for (_uint j = 0; j < anim->mNumChannels; ++j)
		{
			aiNodeAnim* nodeAnim = anim->mChannels[j];

			CAnimationClip::NodeTrack track = {};

			track.nodeName = CEngineString::StringToWString(nodeAnim->mNodeName.C_Str());

			_uint maxKeyCount = max
			(
				nodeAnim->mNumPositionKeys,
				max(nodeAnim->mNumRotationKeys, nodeAnim->mNumScalingKeys)
			);

			for (_uint k = 0; k < maxKeyCount; ++k)
			{
				CAnimationClip::Keyframe key = {};

				if (k < nodeAnim->mNumPositionKeys)
				{
					key.timeStamp = static_cast<_float>(nodeAnim->mPositionKeys[k].mTime);
					key.position = vector3
					(
						nodeAnim->mPositionKeys[k].mValue.x,
						nodeAnim->mPositionKeys[k].mValue.y,
						nodeAnim->mPositionKeys[k].mValue.z
					);
				}

				if (k < nodeAnim->mNumRotationKeys)
				{
					key.rotation = _float4
					(
						nodeAnim->mRotationKeys[k].mValue.x,
						nodeAnim->mRotationKeys[k].mValue.y,
						nodeAnim->mRotationKeys[k].mValue.z,
						nodeAnim->mRotationKeys[k].mValue.w
					);
				}

				if (k < nodeAnim->mNumScalingKeys)
				{
					key.scaling = vector3
					(
						nodeAnim->mScalingKeys[k].mValue.x,
						nodeAnim->mScalingKeys[k].mValue.y,
						nodeAnim->mScalingKeys[k].mValue.z
					);
				}

				track.keyframes.push_back(key);
			}

			animInfo.tracks.push_back(track);
		}

		animationInfoList.push_back(animInfo);
	}

	auto splitPath = CEngineString::Split(_filePath, L"/");

	wstring fileFolder = splitPath[splitPath.size() - 2];
	wstring fileNameExt = splitPath[splitPath.size() - 1];

	auto pureName = CEngineString::Split(fileNameExt, L".")[0];

	wstring saveName = fileFolder + L"_" + pureName;

	if (FAILED(SaveAnimationClipBufferInfos(L"BinaryAssets/AnimationClipData/" + saveName + L".animdata", animationInfoList)))
	{
		CDebug::LogError(L"Failed ceate animation clip Data - can not save: " + _filePath);
		return E_FAIL;
	}

	CDebug::Log(L"Complete ceate animation clip Data: " + _filePath);

	return S_OK;
}

HRESULT CResources::ConvertOTFTTFToSpriteFont(const wstring _filePath)
{
	// 1. 실행파일 위치 얻기
	wchar_t exeDir[MAX_PATH] = {};
	GetModuleFileNameW(NULL, exeDir, MAX_PATH);
	PathRemoveFileSpecW(exeDir);

	// 2. 상대경로를 절대경로로 변환
	wchar_t fullFontPath[MAX_PATH] = {};
	wcscpy_s(fullFontPath, exeDir);
	PathAppendW(fullFontPath, _filePath.c_str());

	wchar_t absoluteFontPath[MAX_PATH] = {};
	if (!GetFullPathNameW(fullFontPath, MAX_PATH, absoluteFontPath, nullptr))
	{
		CDebug::LogError("GetFullPathName failed.");
		return E_FAIL;
	}

	// 3. %WINDIR%\Fonts 폴더로 복사
	wchar_t fontsDir[MAX_PATH] = {};
	GetWindowsDirectoryW(fontsDir, MAX_PATH);
	PathAppendW(fontsDir, L"Fonts");

	const wchar_t* fontFileName = PathFindFileNameW(absoluteFontPath);
	wchar_t installedFontPath[MAX_PATH] = {};
	PathCombineW(installedFontPath, fontsDir, fontFileName);

	if (!CopyFileW(absoluteFontPath, installedFontPath, FALSE))
	{
		CDebug::LogError(L"Failed to copy to Fonts folder. Error: " + GetLastError());
		return E_FAIL;
	}

	// 4. 폰트 등록
	if (AddFontResourceExW(installedFontPath, FR_NOT_ENUM, 0) == 0)
	{
		CDebug::LogError("AddFontResourceExW failed");
		return E_FAIL;
	}
	SendMessageW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);

	wstring fixPath = CEngineString::Replace(_filePath, L"/", L"\\");
	auto splitPath = CEngineString::Split(_filePath, L"\\");
	wstring onlyFileName = CEngineString::Split(splitPath[splitPath.size() - 1], L".")[0];

	wstring outfilePath = L"BinaryAssets/FontData/" + onlyFileName + L".spritefont";
	outfilePath = CEngineString::Replace(outfilePath, L"/", L"\\");

	CDebug::Log(L"OutFilePath: " + outfilePath);

	// 5. 출력 파일 경로 (예시로 동일 위치에 저장)
	wchar_t spriteOutput[MAX_PATH] = {};
	wcscpy_s(spriteOutput, exeDir);
	PathAppendW(spriteOutput, outfilePath.c_str());

	wchar_t outputFullPath[MAX_PATH] = {};
	GetFullPathNameW(spriteOutput, MAX_PATH, outputFullPath, nullptr);

	// 6. MakeSpriteFont.exe 실행 (폰트 이름으로 호출해야 함)
	wstring cmdLine = L"\"";
	cmdLine += exeDir;
	cmdLine += L"\\..\\..\\Engine\\Tools\\MakeSpriteFont.exe\" /FontSize:32 /FontStyle:Regular ";
	cmdLine += L"\"Liberation Sans\" ";  // 실제 폰트 패밀리 이름
	cmdLine += L"\"" + wstring(outputFullPath) + L"\"";

	CDebug::Log(L"[RUNNING]: " + cmdLine);

	STARTUPINFOW si{ sizeof(si) };
	PROCESS_INFORMATION pi{};
	vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
	cmdBuf.push_back(L'\0');

	if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, exeDir, &si, &pi))
	{
		CDebug::LogError(L"CreateProcess failed. Error: " + GetLastError());
		RemoveFontResourceExW(installedFontPath, FR_NOT_ENUM, 0);
		DeleteFileW(installedFontPath);
		return E_FAIL;
	}

	WaitForSingleObject(pi.hProcess, INFINITE);
	DWORD exitCode = 0;
	GetExitCodeProcess(pi.hProcess, &exitCode);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	// 7. 폰트 제거 및 파일 삭제
	RemoveFontResourceExW(installedFontPath, FR_NOT_ENUM, 0);
	SendMessageW(HWND_BROADCAST, WM_FONTCHANGE, 0, 0);
	DeleteFileW(installedFontPath);

	if (exitCode != 0)
	{
		CDebug::LogError("MakeSpriteFont.exe failed with exit code: " + exitCode);
		return E_FAIL;
	}

	CDebug::Log(L"SpriteFont successfully created at: " + wstring(outputFullPath));
	
	return S_OK;
}

HRESULT CResources::SaveSceneObjectTransformInfos(const wstring _filePath, vector<CScene::ObjectsTransformInfo> _infoList)
{
	using namespace std;

	ofstream out(_filePath, ios::binary);

	if (!out.is_open())
	{
		CDebug::LogError(L"SaveSceneObjectTransformInfos failed - can not open: " + _filePath);
		return E_FAIL;
	}

	const _uint magic = 0x53434E32;
	const _uint version = 2;
	_uint count = static_cast<_uint>(_infoList.size());
	out.write(reinterpret_cast<const char*>(&magic), sizeof(_uint));
	out.write(reinterpret_cast<const char*>(&version), sizeof(_uint));
	out.write(reinterpret_cast<const char*>(&count), sizeof(_uint));

	for (_uint i = 0; i < count; ++i)
	{
		CScene::ObjectsTransformInfo info = _infoList[i];

		out.write(reinterpret_cast<const char*>(&info.objID), sizeof(_uint));

		_uint guidSize = static_cast<_uint>(info.objGuid.size());
		out.write(reinterpret_cast<const char*>(&guidSize), sizeof(_uint));
		if (guidSize > 0)
			out.write(reinterpret_cast<const char*>(info.objGuid.data()), sizeof(wchar_t) * guidSize);

		_uint nameSize = static_cast<_uint>(info.objName.size());
		out.write(reinterpret_cast<const char*>(&nameSize), sizeof(_uint));
		if (nameSize > 0)
			out.write(reinterpret_cast<const char*>(info.objName.data()), sizeof(wchar_t) * nameSize);
		out.write(reinterpret_cast<const char*>(&info.localPos), sizeof(_float3));
		out.write(reinterpret_cast<const char*>(&info.localQuaternion), sizeof(_float4));
		out.write(reinterpret_cast<const char*>(&info.localScale), sizeof(_float3));

		out.write(reinterpret_cast<const char*>(&info.isRect), sizeof(_bool));
		if (info.isRect)
		{
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchoredPos), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.widthHeight), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.pivot), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchorMin), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchorMax), sizeof(_float2));
		}
	}

	out.close();

	CDebug::Log(L"Save complete scenedata: " + _filePath);

	return S_OK;
}

vector<CScene::ObjectsTransformInfo> CResources::ReadSceneObjectTransformInfos(const wstring _binFileName)
{
	using namespace std;

	vector<CScene::ObjectsTransformInfo> resultInfo = {};

	ifstream in(L"BinaryAssets/SceneData/" + _binFileName, ios::binary);

	if (!in.is_open())
		return {};

	_uint count = 0;
	_uint version = 1;
	_uint firstValue = 0;
	in.read(reinterpret_cast<char*>(&firstValue), sizeof(_uint));

	if (firstValue == 0x53434E32)
	{
		in.read(reinterpret_cast<char*>(&version), sizeof(_uint));
		in.read(reinterpret_cast<char*>(&count), sizeof(_uint));
	}
	else
	{
		count = firstValue;
	}

	for (_uint i = 0; i < count; ++i)
	{
		CScene::ObjectsTransformInfo info = {};

		_uint objId = 0;
		in.read(reinterpret_cast<char*>(&objId), sizeof(_uint));
		info.objID = objId;

		if (version >= 2)
		{
			_uint guidSize = 0;
			in.read(reinterpret_cast<char*>(&guidSize), sizeof(_uint));
			if (guidSize > 0)
			{
				wstring temp(guidSize, L'\0');
				in.read(reinterpret_cast<char*>(&temp[0]), sizeof(wchar_t) * guidSize);
				info.objGuid = move(temp);
			}
		}

		_uint nameSize = 0;
		in.read(reinterpret_cast<char*>(&nameSize), sizeof(_uint));
		if (nameSize > 0)
		{
			wstring temp(nameSize, L'\0');
			in.read(reinterpret_cast<char*>(&temp[0]), sizeof(wchar_t) * nameSize);
			info.objName = move(temp);
		}
		else
			info.objName = L"";

		_float3 pos = {};
		in.read(reinterpret_cast<char*>(&info.localPos), sizeof(_float3));
		in.read(reinterpret_cast<char*>(&info.localQuaternion), sizeof(_float4));
		in.read(reinterpret_cast<char*>(&info.localScale), sizeof(_float3));

		_bool isRect = false;
		in.read(reinterpret_cast<char*>(&info.isRect), sizeof(_bool));
		
		if (info.isRect)
		{
			in.read(reinterpret_cast<char*>(&info.rectInfo.anchoredPos), sizeof(_float2));
			in.read(reinterpret_cast<char*>(&info.rectInfo.widthHeight), sizeof(_float2));
			in.read(reinterpret_cast<char*>(&info.rectInfo.pivot), sizeof(_float2));
			in.read(reinterpret_cast<char*>(&info.rectInfo.anchorMin), sizeof(_float2));
			in.read(reinterpret_cast<char*>(&info.rectInfo.anchorMax), sizeof(_float2));
		}

		resultInfo.push_back(info);
	}

	in.close();

	return resultInfo;
}

HRESULT CResources::SaveMeshBufferInfos(const wstring _filePath, vector<CMeshBuffer::MeshBufferInitiaizeInfo> _infoList)
{
	using namespace std;

	ofstream out(_filePath, ios::binary);

	if (!out.is_open())
		return E_FAIL;

	_uint count = static_cast<_uint>(_infoList.size());
	out.write(reinterpret_cast<const char*>(&count), sizeof(_uint));

	for (const auto& info : _infoList)
	{
		_uint nameSize = static_cast<_uint>(info.meshName.size());
		out.write(reinterpret_cast<char*>(&nameSize), sizeof(_uint));
		if (nameSize > 0)
			out.write(reinterpret_cast<const char*>(info.meshName.data()), sizeof(wchar_t) * nameSize);

		_uint bufferSize = static_cast<_uint>(info.buffer.size());
		out.write(reinterpret_cast<char*>(&bufferSize), sizeof(_uint));
		if (bufferSize > 0)
			out.write(reinterpret_cast<const char*>(info.buffer.data()), bufferSize);

		_uint indicesSize = static_cast<_uint>(info.indices.size());
		out.write(reinterpret_cast<char*>(&indicesSize), sizeof(_uint));
		if (indicesSize > 0)
			out.write(reinterpret_cast<const char*>(info.indices.data()), sizeof(_uint) * indicesSize);

		out.write(reinterpret_cast<const char*>(&info.desc), sizeof(CMeshBuffer::MESHBUFFERDESC));

		_uint diffuseTexPathSize = static_cast<_uint>(info.diffuseMapPath.size());
		out.write(reinterpret_cast<char*>(&diffuseTexPathSize), sizeof(_uint));
		if (diffuseTexPathSize > 0)
			out.write(reinterpret_cast<const char*>(info.diffuseMapPath.data()), sizeof(wchar_t) * diffuseTexPathSize);
	}

	out.close();

	CDebug::Log(L"Save complete meshdata: " + _filePath);

	return S_OK;
}

vector<CMeshBuffer::MeshBufferInitiaizeInfo> CResources::ReadMeshBufferInfos(const wstring _binFileName)
{
	vector<CMeshBuffer::MeshBufferInitiaizeInfo> infoList = {};

	using namespace std;

	ifstream in(L"BinaryAssets/MeshData/" + _binFileName, ios::binary);

	if (!in.is_open())
	{
		CDebug::LogError(L"ReadMeshBufferInfos failed - can not open: " + _binFileName);
		return {};
	}

	_uint count = 0;
	in.read(reinterpret_cast<char*>(&count), sizeof(_uint));

	for (_uint i = 0; i < count; ++i)
	{
		CMeshBuffer::MeshBufferInitiaizeInfo info = {};

		_uint nameCount = 0;
		in.read(reinterpret_cast<char*>(&nameCount), sizeof(_uint));
		if (nameCount > 0)
		{
			info.meshName.resize(nameCount);
			in.read(reinterpret_cast<char*>(info.meshName.data()), sizeof(wchar_t) * nameCount);
		}

		_uint bufferSize = 0;
		in.read(reinterpret_cast<char*>(&bufferSize), sizeof(_uint));
		if (bufferSize > 0)
		{
			info.buffer.resize(bufferSize);
			in.read(reinterpret_cast<char*>(info.buffer.data()), bufferSize);
		}

		_uint indexCount = 0;
		in.read(reinterpret_cast<char*>(&indexCount), sizeof(_uint));
		if (indexCount > 0)
		{
			info.indices.resize(indexCount);
			in.read(reinterpret_cast<char*>(info.indices.data()), sizeof(_uint) * indexCount);
		}

		in.read(reinterpret_cast<char*>(&info.desc), sizeof(CMeshBuffer::MESHBUFFERDESC));

		_uint diffuseTexPathCount = 0;
		in.read(reinterpret_cast<char*>(&diffuseTexPathCount), sizeof(_uint));
		if (diffuseTexPathCount > 0)
		{
			info.diffuseMapPath.resize(diffuseTexPathCount);
			in.read(reinterpret_cast<char*>(info.diffuseMapPath.data()), sizeof(wchar_t) * diffuseTexPathCount);
		}

		infoList.push_back(info);
	}

	in.close();

	return infoList;
}

HRESULT CResources::SaveSkinnedBufferInfos(const wstring _filePath, vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> _infoList, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _skeletonInfo)
{
	using namespace std;

	ofstream out(_filePath, ios::binary);

	if (!out.is_open())
		return E_FAIL;

	_uint count = static_cast<_uint>(_infoList.size());
	out.write(reinterpret_cast<const char*>(&count), sizeof(_uint));

	for (const auto& info : _infoList)
	{
		_uint nameSize = static_cast<_uint>(info.meshName.size());
		out.write(reinterpret_cast<char*>(&nameSize), sizeof(_uint));
		if (nameSize > 0)
			out.write(reinterpret_cast<const char*>(info.meshName.data()), sizeof(wchar_t) * nameSize);

		_uint bufferSize = static_cast<_uint>(info.buffer.size());
		out.write(reinterpret_cast<char*>(&bufferSize), sizeof(_uint));
		if (bufferSize > 0)
			out.write(reinterpret_cast<const char*>(info.buffer.data()), bufferSize);

		_uint indicesSize = static_cast<_uint>(info.indices.size());
		out.write(reinterpret_cast<char*>(&indicesSize), sizeof(_uint));
		if (indicesSize > 0)
			out.write(reinterpret_cast<const char*>(info.indices.data()), sizeof(_uint) * indicesSize);

		out.write(reinterpret_cast<const char*>(&info.desc), sizeof(CMeshBuffer::MESHBUFFERDESC));

		_uint diffuseTexPathSize = static_cast<_uint>(info.diffuseMapPath.size());
		out.write(reinterpret_cast<char*>(&diffuseTexPathSize), sizeof(_uint));
		if (diffuseTexPathSize > 0)
			out.write(reinterpret_cast<const char*>(info.diffuseMapPath.data()), sizeof(wchar_t) * diffuseTexPathSize);

		_uint boneNamesSize = static_cast<_uint>(info.boneNames.size());
		out.write(reinterpret_cast<char*>(&boneNamesSize), sizeof(_uint));
		if (boneNamesSize > 0)
		{
			for (_uint i = 0; i < boneNamesSize; ++i)
			{
				_uint size = static_cast<_uint>(info.boneNames[i].size());
				out.write(reinterpret_cast<char*>(&size), sizeof(_uint));
				out.write(reinterpret_cast<const char*>(info.boneNames[i].data()), sizeof(wchar_t) * size);
			}
		}

		_uint boneMatricesSize = static_cast<_uint>(info.boneOffsetMatrices.size());
		out.write(reinterpret_cast<char*>(&boneMatricesSize), sizeof(_uint));
		if (boneMatricesSize > 0)
			out.write(reinterpret_cast<const char*>(info.boneOffsetMatrices.data()), sizeof(_float4x4) * boneMatricesSize);
	}

	_uint skeletalCount = static_cast<_uint>(_skeletonInfo.size());
	out.write(reinterpret_cast<const char*>(&skeletalCount), sizeof(_uint));
	for (const auto& bone : _skeletonInfo)
	{
		out.write(reinterpret_cast<const char*>(&bone.nodeId), sizeof(_uint));

		_uint nameLen = static_cast<_uint>(bone.name.size());
		out.write(reinterpret_cast<const char*>(&nameLen), sizeof(_uint));
		if (nameLen > 0)
			out.write(reinterpret_cast<const char*>(bone.name.data()), sizeof(wchar_t) * nameLen);

		out.write(reinterpret_cast<const char*>(&bone.transformation), sizeof(_float4x4));
		out.write(reinterpret_cast<const char*>(&bone.parentId), sizeof(_int));

		_uint numMeshes = bone.numMeshes;
		out.write(reinterpret_cast<const char*>(&numMeshes), sizeof(_uint));

		// child info
		_uint childLen = static_cast<_uint>(bone.childsId.size());
		out.write(reinterpret_cast<const char*>(&childLen), sizeof(_uint));
		if (childLen > 0)
			out.write(reinterpret_cast<const char*>(bone.childsId.data()), sizeof(_int) * childLen);

		// mesh info
		_uint meshLen = static_cast<_uint>(bone.meshsId.size());
		out.write(reinterpret_cast<const char*>(&meshLen), sizeof(_uint));
		if (meshLen > 0)
			out.write(reinterpret_cast<const char*>(bone.meshsId.data()), sizeof(_int) * meshLen);
	}

	out.close();

	CDebug::Log(L"Save complete skinneddata: " + _filePath);

	return S_OK;
}

CSkinnedMeshBuffer::SkinnedBuffer CResources::ReadSkinnedBufferInfos(const wstring _binFileName)
{
	using namespace std;

	CSkinnedMeshBuffer::SkinnedBuffer resultBuffer = {};

	vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> infoList = {};

	ifstream in(L"BinaryAssets/SkinnedMeshData/" + _binFileName, ios::binary);

	if (!in.is_open())
	{
		CDebug::LogError(L"ReadSkinnedBufferInfos failed - can not open: " + _binFileName);
		return {};
	}

	_uint count = 0;
	in.read(reinterpret_cast<char*>(&count), sizeof(_uint));

	for (_uint i = 0; i < count; ++i)
	{
		CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo info = {};

		_uint nameCount = 0;
		in.read(reinterpret_cast<char*>(&nameCount), sizeof(_uint));
		if (nameCount > 0)
		{
			info.meshName.resize(nameCount);
			in.read(reinterpret_cast<char*>(info.meshName.data()), sizeof(wchar_t) * nameCount);
		}

		_uint bufferSize = 0;
		in.read(reinterpret_cast<char*>(&bufferSize), sizeof(_uint));
		if (bufferSize > 0)
		{
			info.buffer.resize(bufferSize);
			in.read(reinterpret_cast<char*>(info.buffer.data()), bufferSize);
		}

		_uint indexCount = 0;
		in.read(reinterpret_cast<char*>(&indexCount), sizeof(_uint));
		if (indexCount > 0)
		{
			info.indices.resize(indexCount);
			in.read(reinterpret_cast<char*>(info.indices.data()), sizeof(_uint) * indexCount);
		}

		in.read(reinterpret_cast<char*>(&info.desc), sizeof(CMeshBuffer::MESHBUFFERDESC));

		_uint diffuseTexPathCount = 0;
		in.read(reinterpret_cast<char*>(&diffuseTexPathCount), sizeof(_uint));
		if (diffuseTexPathCount > 0)
		{
			info.diffuseMapPath.resize(diffuseTexPathCount);
			in.read(reinterpret_cast<char*>(info.diffuseMapPath.data()), sizeof(wchar_t) * diffuseTexPathCount);
		}

		_uint boneNamesCount = 0;
		in.read(reinterpret_cast<char*>(&boneNamesCount), sizeof(_uint));
		if (boneNamesCount > 0)
		{
			for (_uint i = 0; i < boneNamesCount; ++i)
			{
				_uint size = 0;
				in.read(reinterpret_cast<char*>(&size), sizeof(_uint));

				wstring name;
				if (size > 0)
				{
					name.resize(size);
					in.read(reinterpret_cast<char*>(name.data()), sizeof(wchar_t) * size);
				}

				info.boneNames.push_back(name);
			}
		}

		_uint boneMatrixCount = 0;
		in.read(reinterpret_cast<char*>(&boneMatrixCount), sizeof(_uint));
		if (boneMatrixCount > 0)
		{
			for (_uint i = 0; i < boneMatrixCount; ++i)
			{
				_float4x4 matrix = {};
				in.read(reinterpret_cast<char*>(&matrix), sizeof(_float4x4));
				info.boneOffsetMatrices.push_back(matrix);
			}
		}

		infoList.push_back(info);
	}

	vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> skeletalList = {};

	_uint skeletalCount = 0;
	in.read(reinterpret_cast<char*>(&skeletalCount), sizeof(_uint));

	for (_uint i = 0; i < skeletalCount; ++i)
	{
		CSkinnedMeshBuffer::SKINNEDSKELETAL skeletal{};
		in.read(reinterpret_cast<char*>(&skeletal.nodeId), sizeof(_uint));

		_uint nameLen = 0;
		in.read(reinterpret_cast<char*>(&nameLen), sizeof(_uint));
		if (nameLen > 0)
		{
			skeletal.name.resize(nameLen);
			in.read(reinterpret_cast<char*>(skeletal.name.data()), sizeof(wchar_t) * nameLen);
		}

		in.read(reinterpret_cast<char*>(&skeletal.transformation), sizeof(_float4x4));
		in.read(reinterpret_cast<char*>(&skeletal.parentId), sizeof(_int));
		in.read(reinterpret_cast<char*>(&skeletal.numMeshes), sizeof(_uint));

		_uint childCount = 0;
		in.read(reinterpret_cast<char*>(&childCount), sizeof(_uint));
		if (childCount > 0)
		{
			skeletal.childsId.resize(childCount);
			in.read(reinterpret_cast<char*>(skeletal.childsId.data()), sizeof(_int) * childCount);
		}
		skeletal.numChild = childCount;

		skeletalList.push_back(skeletal);

		_uint meshCount = 0;
		in.read(reinterpret_cast<char*>(&meshCount), sizeof(_uint));
		skeletal.numMeshes = meshCount;
		if (meshCount > 0)
		{
			skeletal.meshsId.resize(meshCount);
			in.read(reinterpret_cast<char*>(skeletal.meshsId.data()), sizeof(_int) * meshCount);
		}
	}

	resultBuffer.initList = infoList;
	resultBuffer.skeletalList = skeletalList;

	in.close();

	return resultBuffer;
}

HRESULT CResources::SaveAnimationClipBufferInfos(const wstring _filePath, vector<CAnimationClip::AnimationClipInitInfo> _infoList)
{
	using namespace std;

	ofstream out(_filePath, ios::binary);
	if (!out.is_open())
		return E_FAIL;

	_uint clipCount = static_cast<_uint>(_infoList.size());
	out.write(reinterpret_cast<const char*>(&clipCount), sizeof(_uint));

	for (const auto& clip : _infoList)
	{
		_uint nameLen = static_cast<_uint>(clip.name.size());
		out.write(reinterpret_cast<const char*>(&nameLen), sizeof(_uint));
		if (nameLen)
			out.write(reinterpret_cast<const char*>(clip.name.data()),
				sizeof(wchar_t) * nameLen);

		out.write(reinterpret_cast<const char*>(&clip.duration), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&clip.ticksPerSecond), sizeof(_float));

		_uint trackCount = static_cast<_uint>(clip.tracks.size());
		out.write(reinterpret_cast<const char*>(&trackCount), sizeof(_uint));

		for (const auto& track : clip.tracks)
		{
			_uint nodeLen = static_cast<_uint>(track.nodeName.size());
			out.write(reinterpret_cast<const char*>(&nodeLen), sizeof(_uint));
			if (nodeLen)
				out.write(reinterpret_cast<const char*>(track.nodeName.data()),
					sizeof(wchar_t) * nodeLen);

			_uint keyCount = static_cast<_uint>(track.keyframes.size());
			out.write(reinterpret_cast<const char*>(&keyCount), sizeof(_uint));

			for (const auto& key : track.keyframes)
			{
				out.write(reinterpret_cast<const char*>(&key.timeStamp), sizeof(double));
				out.write(reinterpret_cast<const char*>(&key.position), sizeof(vector3));
				out.write(reinterpret_cast<const char*>(&key.rotation), sizeof(_float4));
				out.write(reinterpret_cast<const char*>(&key.scaling), sizeof(vector3));
			}
		}
	}

	out.close();
	CDebug::Log(L"Save complete animation clip data: " + _filePath);

	return S_OK;
}

vector<CAnimationClip::AnimationClipInitInfo> CResources::ReadAnimationClipBufferInfos(const wstring _binFileName)
{
	using namespace std;
	vector<CAnimationClip::AnimationClipInitInfo> clips;

	ifstream in(L"BinaryAssets/AnimationClipData/" + _binFileName, ios::binary);
	if (!in.is_open())
	{
		CDebug::LogError(L"ReadAnimationClipBufferInfos failed - can not open: " + _binFileName);
		return {};
	}
	_uint clipCount = 0;
	in.read(reinterpret_cast<char*>(&clipCount), sizeof(_uint));
	clips.reserve(clipCount);

	for (_uint c = 0; c < clipCount; ++c)
	{
		CAnimationClip::AnimationClipInitInfo clip{};

		_uint nameLen = 0;
		in.read(reinterpret_cast<char*>(&nameLen), sizeof(_uint));
		if (nameLen)
		{
			clip.name.resize(nameLen);
			in.read(reinterpret_cast<char*>(clip.name.data()),
				sizeof(wchar_t) * nameLen);
		}

		in.read(reinterpret_cast<char*>(&clip.duration), sizeof(_float));
		in.read(reinterpret_cast<char*>(&clip.ticksPerSecond), sizeof(_float));

		_uint trackCount = 0;
		in.read(reinterpret_cast<char*>(&trackCount), sizeof(_uint));
		clip.tracks.reserve(trackCount);

		for (_uint t = 0; t < trackCount; ++t)
		{
			CAnimationClip::NodeTrack track{};

			_uint nodeLen = 0;
			in.read(reinterpret_cast<char*>(&nodeLen), sizeof(_uint));
			if (nodeLen)
			{
				track.nodeName.resize(nodeLen);
				in.read(reinterpret_cast<char*>(track.nodeName.data()),
					sizeof(wchar_t) * nodeLen);
			}

			_uint keyCount = 0;
			in.read(reinterpret_cast<char*>(&keyCount), sizeof(_uint));
			track.keyframes.reserve(keyCount);

			for (_uint k = 0; k < keyCount; ++k)
			{
				CAnimationClip::Keyframe key{};
				in.read(reinterpret_cast<char*>(&key.timeStamp), sizeof(_double));
				in.read(reinterpret_cast<char*>(&key.position), sizeof(vector3));
				in.read(reinterpret_cast<char*>(&key.rotation), sizeof(_float4));
				in.read(reinterpret_cast<char*>(&key.scaling), sizeof(vector3));
				track.keyframes.emplace_back(move(key));
			}

			clip.tracks.emplace_back(move(track));
		}

		clips.emplace_back(move(clip));

		for (auto& tr : clip.tracks)
			if (tr.nodeName == L"FantasyWolf_")
				for (auto& k : tr.keyframes)
					printf("k.t=%.2f p=(%.2f,%.2f,%.2f)\n",
						k.timeStamp, k.position.x, k.position.y, k.position.z);

		int a = 0;
	}

	in.close();

	return clips;
}


CAnimatorController::AnimatorControllerInitInfo CResources::ReadAnimatorControllerBufferInfos(const wstring _binFileName)
{
	using namespace std;
	CAnimatorController::AnimatorControllerInitInfo info{};

	ifstream in(L"BinaryAssets/AnimatorControllerData/" + _binFileName, ios::binary);
	if (!in.is_open())
	{
		CDebug::LogError(L"ReadAnimationAnimatoinControllerBufferInfos failed - can not open: " + _binFileName);
		return {};
	}

	_uint magic = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(_uint));
    const _bool hasGraphData = (magic == 0x41434232 || magic == 0x41434233 || magic == 0x41434234 || magic == 0x41434235);
    const _bool hasEntryTransitions = (magic == 0x41434233 || magic == 0x41434234 || magic == 0x41434235);
    const _bool hasBlendTree = (magic == 0x41434234 || magic == 0x41434235);
    const _bool hasDirectBlend = (magic == 0x41434235);
    if (magic != 0x41434231 && !hasGraphData)
    {
        CDebug::LogError(L"ReadAnimationAnimatoinControllerBufferInfos failed - invalid magic: " + _binFileName);
        return {};
    }

	auto readWString = [&in]()
		{
			wstring ws;
			_uint len = 0;
			in.read(reinterpret_cast<char*>(&len), sizeof(_uint));
			if (len)
			{
				ws.resize(len);
				in.read(reinterpret_cast<char*>(ws.data()), sizeof(wchar_t) * len);
			}
			return ws;
		};

	info.controllerName = readWString();
	info.entryState = readWString();
	if (hasGraphData)
	{
		in.read(reinterpret_cast<char*>(&info.entryPos), sizeof(_float2));
		in.read(reinterpret_cast<char*>(&info.anyStatePos), sizeof(_float2));
	}

	_uint paramCount = 0;
	in.read(reinterpret_cast<char*>(&paramCount), sizeof(_uint));
	info.parameters.reserve(paramCount);
	for (_uint i = 0; i < paramCount; ++i)
	{
		CAnimatorController::ParameterDesc p{};
		p.name = readWString();

		_uint type = 0;
		in.read(reinterpret_cast<char*>(&type), sizeof(_uint));
		p.type = static_cast<CAnimatorController::PARAM_TYPE>(type);

		in.read(reinterpret_cast<char*>(&p.defaultBool), sizeof(_bool));
		in.read(reinterpret_cast<char*>(&p.defaultInt), sizeof(_int));
		in.read(reinterpret_cast<char*>(&p.defaultFloat), sizeof(_float));

		info.parameters.emplace_back(move(p));
	}

	_uint stateCount = 0;
	in.read(reinterpret_cast<char*>(&stateCount), sizeof(_uint));
	info.states.reserve(stateCount);
	for (_uint s = 0; s < stateCount; ++s)
	{
		CAnimatorController::State st{};
		st.name = readWString();
		st.motionName = readWString();
		if (hasBlendTree)
		{
			_uint motionType = 0;
			in.read(reinterpret_cast<char*>(&motionType), sizeof(_uint));
			st.motionType = static_cast<CAnimatorController::STATE_MOTION_TYPE>(motionType);
			if (st.motionType == CAnimatorController::STATE_MOTION_TYPE::BLEND_TREE)
			{
                _uint treeType = 0;
                in.read(reinterpret_cast<char*>(&treeType), sizeof(_uint));
                st.blendTree.type = static_cast<CAnimatorController::BLEND_TREE_TYPE>(treeType);
                st.blendTree.paramX = readWString();
                st.blendTree.paramY = readWString();
                if (hasDirectBlend)
                    in.read(reinterpret_cast<char*>(&st.blendTree.directBlendDuration), sizeof(_float));
                else
                    st.blendTree.directBlendDuration = 0.f;
                _uint childCount = 0;
				in.read(reinterpret_cast<char*>(&childCount), sizeof(_uint));
				st.blendTree.children.reserve(childCount);
				for (_uint c = 0; c < childCount; ++c)
				{
					CAnimatorController::State::BlendTreeChild child{};
					child.motionName = readWString();
					in.read(reinterpret_cast<char*>(&child.threshold), sizeof(_float));
					in.read(reinterpret_cast<char*>(&child.position), sizeof(_float2));
					child.directParam = readWString();
					st.blendTree.children.emplace_back(move(child));
				}
			}
		}

		in.read(reinterpret_cast<char*>(&st.speedMul), sizeof(_float));
		if (hasGraphData)
			in.read(reinterpret_cast<char*>(&st.pos), sizeof(_float2));

		_uint trCount = 0;
		in.read(reinterpret_cast<char*>(&trCount), sizeof(_uint));
		st.transitions.reserve(trCount);
		for (_uint t = 0; t < trCount; ++t)
		{
			CAnimatorController::Transition tr{};
			tr.toState = readWString();

			in.read(reinterpret_cast<char*>(&tr.blendDuration), sizeof(_float));
			in.read(reinterpret_cast<char*>(&tr.hasExitTime), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&tr.exitTimeNormalized), sizeof(_float));

			_uint condCount = 0;
			in.read(reinterpret_cast<char*>(&condCount), sizeof(_uint));
			tr.conditions.reserve(condCount);
			for (_uint c = 0; c < condCount; ++c)
			{
				CAnimatorController::Condition cond{};
				cond.paramName = readWString();

				_uint op = 0;
				in.read(reinterpret_cast<char*>(&op), sizeof(_uint));
				cond.op = static_cast<CAnimatorController::COMPARE_OP>(op);

				in.read(reinterpret_cast<char*>(&cond.b), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&cond.i), sizeof(_int));
				in.read(reinterpret_cast<char*>(&cond.f), sizeof(_float));

				tr.conditions.emplace_back(move(cond));
			}

			st.transitions.emplace_back(move(tr));
		}

		info.states.emplace_back(move(st));
	}

	_uint anyCount = 0;
	in.read(reinterpret_cast<char*>(&anyCount), sizeof(_uint));
	info.anyStateTransitions.reserve(anyCount);
	for (_uint a = 0; a < anyCount; ++a)
	{
		CAnimatorController::Transition tr{};
		tr.toState = readWString();

		in.read(reinterpret_cast<char*>(&tr.blendDuration), sizeof(_float));
		in.read(reinterpret_cast<char*>(&tr.hasExitTime), sizeof(_bool));
		in.read(reinterpret_cast<char*>(&tr.exitTimeNormalized), sizeof(_float));

		_uint condCount = 0;
		in.read(reinterpret_cast<char*>(&condCount), sizeof(_uint));
		tr.conditions.reserve(condCount);
		for (_uint c = 0; c < condCount; ++c)
		{
			CAnimatorController::Condition cond{};
			cond.paramName = readWString();

			_uint op = 0;
			in.read(reinterpret_cast<char*>(&op), sizeof(_uint));
			cond.op = static_cast<CAnimatorController::COMPARE_OP>(op);

			in.read(reinterpret_cast<char*>(&cond.b), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&cond.i), sizeof(_int));
			in.read(reinterpret_cast<char*>(&cond.f), sizeof(_float));

			tr.conditions.emplace_back(move(cond));
		}

		info.anyStateTransitions.emplace_back(move(tr));
	}

	if (hasEntryTransitions)
	{
		_uint entryCount = 0;
		in.read(reinterpret_cast<char*>(&entryCount), sizeof(_uint));
		info.entryStateTransitions.reserve(entryCount);
		for (_uint a = 0; a < entryCount; ++a)
		{
			CAnimatorController::Transition tr{};
			tr.toState = readWString();

			in.read(reinterpret_cast<char*>(&tr.blendDuration), sizeof(_float));
			in.read(reinterpret_cast<char*>(&tr.hasExitTime), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&tr.exitTimeNormalized), sizeof(_float));

			_uint condCount = 0;
			in.read(reinterpret_cast<char*>(&condCount), sizeof(_uint));
			tr.conditions.reserve(condCount);
			for (_uint c = 0; c < condCount; ++c)
			{
				CAnimatorController::Condition cond{};
				cond.paramName = readWString();

				_uint op = 0;
				in.read(reinterpret_cast<char*>(&op), sizeof(_uint));
				cond.op = static_cast<CAnimatorController::COMPARE_OP>(op);

				in.read(reinterpret_cast<char*>(&cond.b), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&cond.i), sizeof(_int));
				in.read(reinterpret_cast<char*>(&cond.f), sizeof(_float));

				tr.conditions.emplace_back(move(cond));
			}

			info.entryStateTransitions.emplace_back(move(tr));
		}
	}

	in.close();

	return info;
}

CEngineResource* CResources::AddSceneResource(const wstring& _name, CEngineResource* _resource, const _bool _tempScene)
{
	CScene* targetScene = _tempScene ? CSceneManager::GetInstance().Get_TempScene() :
		CSceneManager::GetInstance().Get_CrtScene();

	if (!_tempScene)
		targetScene->Add_Resource(_name, _resource);
	else
		targetScene->Add_TempResource(_name, _resource);

	return _resource;
}

vector<MeshBundle> CResources::CreateSceneMeshBundle(const wstring& _name, vector<CMeshBuffer::MeshBufferInitiaizeInfo> _infoList, _int _filter, void* _desc, const _bool _tempScene)
{
	_float scaleFactor = 1.f;

	if (_desc)
		scaleFactor = *reinterpret_cast<_float*>(_desc);

	vector<MeshBundle> resultList = {};

	for (_uint i = 0; i < _infoList.size(); ++i)
	{
		MeshBundle newBundle;

		if (_filter & FILTER_MESHBUFFER)
		{
			CMeshBuffer* newBuffer = CMeshBuffer::Create();
			newBuffer->Initialize_Custom(_infoList[i], _desc);

			newBundle.meshBuffer = newBuffer;
		}

		if (_filter & FILTER_MATERIAL)
		{
			CTexture* newTex = CTexture::Create();
			newTex->Initialize(_infoList[i].diffuseMapPath, _infoList[i].diffuseMapPath, nullptr);

			newBundle.texture = newTex;
		}

		resultList.push_back(newBundle);
	}

	CScene* targetScene = _tempScene ? CSceneManager::GetInstance().Get_TempScene() :
		CSceneManager::GetInstance().Get_CrtScene();

	if (!_tempScene)
		targetScene->Add_MeshBundle(_name, resultList);
	else
		targetScene->Add_TempMeshBundle(_name, resultList);

	CDebug::Log(L"Create Scene resource successfully: " + _name);

	return resultList;
}

vector<SkinnedMeshBundle> CResources::CreateSceneSkinnedBundle(const wstring& _name, vector<CSkinnedMeshBuffer::SkinnedBufferInitiaizeInfo> _infoList, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> _skelList, _int _filter, void* _desc, const _bool _tempScene)
{
	if (_infoList.size() <= 0)
	{
		CDebug::LogError(L"Failed create SceneSkinnedBundle - Empty list: " + _name);
		return {};
	}

	_float scaleFactor = 1.f;

	if (_desc)
		scaleFactor = *reinterpret_cast<_float*>(_desc);

	vector<SkinnedMeshBundle> resultList = {};

	for (_uint i = 0; i < _infoList.size(); ++i)
	{
		SkinnedMeshBundle newBundle;

		if (_filter & FILTER_MESHBUFFER)
		{
			CSkinnedMeshBuffer* newBuffer = CSkinnedMeshBuffer::Create();
			newBuffer->Initiailize_Custom(_infoList[i], _skelList, _desc);

			newBundle.meshBuffer = newBuffer;
		}

		if (_filter & FILTER_MATERIAL)
		{
			CTexture* newTex = CTexture::Create();
			newTex->Initialize(_infoList[i].diffuseMapPath, _infoList[i].diffuseMapPath, nullptr);

			newBundle.texture = newTex;
		}

		resultList.push_back(newBundle);
	}

	CScene* targetScene = _tempScene ? CSceneManager::GetInstance().Get_TempScene() :
		CSceneManager::GetInstance().Get_CrtScene();

	if (!_tempScene)
	{
		targetScene->Add_SkinnedBundle(_name, resultList);
		targetScene->Add_SkinnedMeshBone(_name, _skelList);
	}
	else
	{
		targetScene->Add_TempSkinnedBundle(_name, resultList);
		targetScene->Add_TempSkinnedMeshBone(_name, _skelList);
	}

	CDebug::Log(L"Create Scene resource successfully: " + _name);

	return resultList;
}

vector<MeshBundle> CResources::LoadMeshBuffersOnScene(const wstring& _name)
{
	vector<MeshBundle> r = {};

	if (CSceneManager::GetInstance().Get_CrtScene())
		r = CSceneManager::GetInstance().Get_CrtScene()->Find_MeshInfoResource(_name);

	return r;
}

vector<SkinnedMeshBundle> CResources::LoadSkinnedMeshBuffersOnScene(const wstring& _name)
{
	vector<SkinnedMeshBundle> r = {};

	if (CSceneManager::GetInstance().Get_CrtScene())
		r = CSceneManager::GetInstance().Get_CrtScene()->Find_SkinnedMeshInfoResource(_name);

	return r;
}

vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> CResources::LoadSkinnedBonesOnScene(const wstring& _name)
{
	vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> r = {};

	if (CSceneManager::GetInstance().Get_CrtScene())
		r = CSceneManager::GetInstance().Get_CrtScene()->Find_SkinnedBonesResource(_name);

	return r;
}

_bool CResources::FileExists(const wstring& _path)
{
	const string path = CEngineString::WStringToString(_path);
	return FileExists(path);
}

_bool CResources::FileExists(const string& _path)
{
	DWORD attrib = GetFileAttributesA(_path.c_str());
	return (attrib != INVALID_FILE_ATTRIBUTES) && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
}

void CResources::Ready_GameResources()
{
	LoadResourceComplete_Game<CMeshBuffer>(L"Line (Mesh Buffer)", L"Line");
	LoadResourceComplete_Game<CMeshBuffer>(L"Rect (Mesh Buffer)", L"Rect");
	LoadResourceComplete_Game<CMeshBuffer>(L"LineRect (Mesh Buffer)", L"LineRect");
	LoadResourceComplete_Game<CMeshBuffer>(L"Cube (Mesh Buffer)", L"Cube");
	LoadResourceComplete_Game<CMeshBuffer>(L"Quad (Mesh Buffer)", L"Quad");

	LoadResourceComplete_Game<CTexture>(L"DefaultSky (Texture)", L"../EngineResources/Image/DefaultSkyBox.png");

	CShader::SHADERDESC lineColorShaderDesc = { L"../EngineResources/Shader/DefaultLine.hlsl", L"",  VertexColorSkinnedBuffer::numElements, VertexColorSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DefaultLine (Shader)", L"", &lineColorShaderDesc);

	CShader* dlShader = LoadOnGame<CShader>(L"DefaultLine (Shader)");
	CMaterial::MATERIALDESC dlMatDesc = { dlShader, false };
	LoadResourceComplete_Game<CMaterial>(L"DefaultLineMaterial (Material)", L"", &dlMatDesc);

	CShader::SHADERDESC litShaderDesc = { L"../EngineResources/Shader/Lit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"Lit (Shader)", L"", &litShaderDesc);

	CShader* litShader = LoadOnGame<CShader>(L"Lit (Shader)");
	CMaterial::MATERIALDESC litMatDesc = { litShader, true };
	litMatDesc.customFloatValues.push_back({ L"gSmoothness", 0.f });
	litMatDesc.customVector2Values.push_back({ L"gTiling", { 1.f, 1.f } });
	litMatDesc.customVector2Values.push_back({ L"gOffset", { 0.f, 0.f } });
	LoadResourceComplete_Game<CMaterial>(L"Lit (Material)", L"", &litMatDesc);

	CShader::SHADERDESC skyBoxShaderDesc = { L"../EngineResources/Shader/Skybox.hlsl", L"",  VertexTexNormalTangentBuffer::numElements, VertexTexNormalTangentBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"SkyBox (Shader)", L"", &skyBoxShaderDesc);

	CShader* skyBoxShader = LoadOnGame<CShader>(L"SkyBox (Shader)");
	CMaterial::MATERIALDESC skyMatDesc = { skyBoxShader, false };
	LoadResourceComplete_Game<CMaterial>(L"SkyBoxMaterial (Material)", L"", &skyMatDesc);

	CSkyBox::SKYBOXBUFFERDESC dskyDesk = { L"DefaultSky (Texture)" };
	LoadResourceComplete_Game<CSkyBox>(L"DefaultSky (SkyBox)", L"", &dskyDesk);

	CShader::SHADERDESC g_BufferlitShaderDesc = { L"../EngineResources/Shader/GBufferLit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"G_BufferLit (Shader)", L"", &g_BufferlitShaderDesc);

	CShader* g_BufferLitShader = LoadOnGame<CShader>(L"G_BufferLit (Shader)");
	CMaterial::MATERIALDESC g_BufferLitMatDesc = { g_BufferLitShader, false, true, true };
	g_BufferLitMatDesc.customFloatValues.push_back({ L"gOcculusion", 1.f });
	g_BufferLitMatDesc.customFloatValues.push_back({ L"gRoughness", 0.5f });
	g_BufferLitMatDesc.customFloatValues.push_back({ L"gMetallic", 0.f });
	g_BufferLitMatDesc.customIntValues.push_back({ L"gObjectID", 0 });
	g_BufferLitMatDesc.customVector2Values.push_back({ L"gTiling", {1.f, 1.f} });
	LoadResourceComplete_Game<CMaterial>(L"G_BufferLit (Material)", L"", &g_BufferLitMatDesc);

	CShader::SHADERDESC deferredPresentShaderDesc = { L"../EngineResources/Shader/DeferredPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	CShader* deferredPresentShader = LoadResourceComplete_Game<CShader>( L"DeferredPresent (Shader)", L"", &deferredPresentShaderDesc);

	CMaterial::MATERIALDESC deferredPresentMatDesc = { deferredPresentShader, false };
	LoadResourceComplete_Game<CMaterial>(L"DeferredPresent (Material)", L"", &deferredPresentMatDesc);

	CShader::SHADERDESC objectIDPresentShaderDesc = { L"../EngineResources/Shader/ObjectIDPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	CShader* objectIDPresentShader = LoadResourceComplete_Game<CShader>(L"ObjectIDPresent (Shader)", L"", &objectIDPresentShaderDesc);

	CMaterial::MATERIALDESC objectIDPresentMatDesc = { objectIDPresentShader, false };
	LoadResourceComplete_Game<CMaterial>(L"ObjectIDPresent (Material)", L"", &objectIDPresentMatDesc);

	CShader::SHADERDESC depthPresentShaderDesc = { L"../EngineResources/Shader/DepthPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	CShader* depthPresentShader = LoadResourceComplete_Game<CShader>(L"DepthPresent (Shader)", L"", &depthPresentShaderDesc);

	CMaterial::MATERIALDESC depthPresentMatDesc = { depthPresentShader, false };
	LoadResourceComplete_Game<CMaterial>(L"DepthPresent (Material)", L"", &depthPresentMatDesc);

	CShader::SHADERDESC shadowDepthPresentShaderDesc = { L"../EngineResources/Shader/ShadowDepthPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"ShadowDepthPresent (Shader)", L"", &shadowDepthPresentShaderDesc);

	CShader* shadowDepthPresentShader = LoadOnGame<CShader>(L"ShadowDepthPresent (Shader)");
	CMaterial::MATERIALDESC shadowDepthPresentMatDesc = { shadowDepthPresentShader, false };
	LoadResourceComplete_Game<CMaterial>(L"ShadowDepthPresent (Material)", L"", &shadowDepthPresentMatDesc);

	CShader::SHADERDESC shadowMaskPresentShaderDesc = { L"../EngineResources/Shader/ShadowMaskPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"ShadowMaskPresent (Shader)", L"", &shadowMaskPresentShaderDesc);

	CShader* shadowMaskPresentShader = LoadOnGame<CShader>(L"ShadowMaskPresent (Shader)");
	CMaterial::MATERIALDESC shadowMaskPresentMatDesc = { shadowMaskPresentShader, false };
	LoadResourceComplete_Game<CMaterial>(L"ShadowMaskPresent (Material)", L"", &shadowMaskPresentMatDesc);

	CShader::SHADERDESC deferredCombineShaderDesc = { L"../EngineResources/Shader/DeferredCombine.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DeferredCombine (Shader)", L"", &deferredCombineShaderDesc);

	CShader* deferredCombineShader = LoadOnGame<CShader>(L"DeferredCombine (Shader)");
	CMaterial::MATERIALDESC deferredCombineMatDesc = { deferredCombineShader, false };
	LoadResourceComplete_Game<CMaterial>(L"DeferredCombine (Material)", L"", &deferredCombineMatDesc);

	CShader::SHADERDESC deferredDiffuseShaderDesc = { L"../EngineResources/Shader/DeferredDiffuse.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DeferredDiffuse (Shader)", L"", &deferredDiffuseShaderDesc);

	CShader* deferredDiffuseShader = LoadOnGame<CShader>(L"DeferredDiffuse (Shader)");
	CMaterial::MATERIALDESC deferredDiffuseMatDesc = { deferredDiffuseShader, true };
	LoadResourceComplete_Game<CMaterial>(L"DeferredDiffuse (Material)", L"", &deferredDiffuseMatDesc);

	CShader::SHADERDESC deferredSpecularShaderDesc = { L"../EngineResources/Shader/DeferredSpecular.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DeferredSpecular (Shader)", L"", &deferredSpecularShaderDesc);

	CShader* deferredSpecularShader = LoadOnGame<CShader>(L"DeferredSpecular (Shader)");
	CMaterial::MATERIALDESC deferredSpecularMatDesc = { deferredSpecularShader, true };
	LoadResourceComplete_Game<CMaterial>(L"DeferredSpecular (Material)", L"", &deferredSpecularMatDesc);

	CShader::SHADERDESC shadowDepthShaderDesc = { L"../EngineResources/Shader/ShadowDepth.hlsl", L"",  VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"ShadowDepth (Shader)", L"", &shadowDepthShaderDesc);

	CShader* shadowDepthShader = LoadOnGame<CShader>(L"ShadowDepth (Shader)");
	CMaterial::MATERIALDESC shadowDepthMatDesc = { shadowDepthShader, false };
	LoadResourceComplete_Game<CMaterial>(L"ShadowDepth (Material)", L"", &shadowDepthMatDesc);

	CShader::SHADERDESC shadowMaskShaderDesc = { L"../EngineResources/Shader/ShadowMask.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"ShadowMask (Shader)", L"", &shadowMaskShaderDesc);

	CShader* shadowMaskShader = LoadOnGame<CShader>(L"ShadowMask (Shader)");
	CMaterial::MATERIALDESC shadowMaskMatDesc = { shadowMaskShader, false };
	LoadResourceComplete_Game<CMaterial>(L"ShadowMask (Material)", L"", &shadowMaskMatDesc);

	CShader::SHADERDESC unlitColorShaderDesc = { L"../EngineResources/Shader/UnlitColor.hlsl", L"",  VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"UnlitColor (Shader)", L"", &unlitColorShaderDesc);

	CShader* ulcShader = LoadOnGame<CShader>(L"UnlitColor (Shader)");
	CMaterial::MATERIALDESC ulcMatDesc = { ulcShader, false };
	LoadResourceComplete_Game<CMaterial>(L"UnlitMaterial (Material)", L"", &ulcMatDesc);

	CShader::SHADERDESC outlineShaderDesc = { L"../EngineResources/Shader/Outline.hlsl", L"",  VertexSkinnedOutlineBuffer::numElements, VertexSkinnedOutlineBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"Outline (Shader)", L"", &outlineShaderDesc);

	CShader::SHADERDESC dUIShaderDesc = { L"../EngineResources/Shader/DefaultUI.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DefaultUI (Shader)", L"", &dUIShaderDesc);

	CShader* duiShader = LoadOnGame<CShader>(L"DefaultUI (Shader)");
	CMaterial::MATERIALDESC duiMatDesc = { duiShader, false };
	LoadResourceComplete_Game<CMaterial>(L"DefaultUIMaterial (Material)", L"", &duiMatDesc);

	wstring dfPath = L"BinaryAssets/FontData/LiberationSans.spritefont";
	LoadResourceComplete_Game<CFont>(L"Sans (Font)", L"", &dfPath);
}

void CResources::TraverseSkeleton(aiNode* _node, _int _parentId, vector<CSkinnedMeshBuffer::SKINNEDSKELETAL>& _outList)
{
	using SKIN = CSkinnedMeshBuffer::SKINNEDSKELETAL;

	SKIN nodeInfo = {};
	nodeInfo.nodeId = static_cast<_int>(_outList.size());
	nodeInfo.parentId = _parentId;
	nodeInfo.name = CEngineString::StringToWString(_node->mName.C_Str());

	// Transform
	aiMatrix4x4 mat = _node->mTransformation;
	nodeInfo.transformation = _float4x4
	(
		mat.a1, mat.b1, mat.c1, mat.d1,
		mat.a2, mat.b2, mat.c2, mat.d2,
		mat.a3, mat.b3, mat.c3, mat.d3,
		mat.a4, mat.b4, mat.c4, mat.d4
	);

	// Mesh indices
	nodeInfo.numMeshes = _node->mNumMeshes;
	for (_uint i = 0; i < _node->mNumMeshes; ++i)
		nodeInfo.meshsId.push_back(_node->mMeshes[i]);

	// 미리 push 해서 자식이 parentId 참고 가능
	_outList.push_back(nodeInfo);
	_int currentId = nodeInfo.nodeId;

	// 자식 노드들 순회
	for (_uint i = 0; i < _node->mNumChildren; ++i)
	{
		// 재귀 이전에 outList size를 얻어 자식 ID 추정
		_int childId = static_cast<_int>(_outList.size());
		_outList[currentId].childsId.push_back(childId);
		_outList[currentId].numChild++;

		TraverseSkeleton(_node->mChildren[i], currentId, _outList);
	}
}
