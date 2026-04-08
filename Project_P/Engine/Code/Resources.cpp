#include "epch.h"
#include "Resources.h"
#include "ScreenGrab.h"
#include "WICTextureLoader.h"

#include <shlobj.h> 
#include <shlwapi.h> 
#include <tchar.h>

#pragma comment(lib, "Shlwapi.lib")

namespace
{
	constexpr UINT kDDSMaxTextureSize = 4096;
	constexpr UINT kDDSMinMipDimension = 64;
	constexpr const wchar_t* kSupportedTextureSourceExtensions[] =
	{
		L".png", L".jpg", L".jpeg", L".bmp", L".tga", L".tif", L".tiff", L".gif"
	};

	constexpr int kSpriteFontFirstChar = 32;
	constexpr int kSpriteFontLastChar = 126;
	constexpr int kSpriteFontPadding = 2;
	constexpr float kSpriteFontPointSize = 32.f;

	wstring ToLowerCopy(wstring value);

	UINT CalculateDDSMipLevels(UINT width, UINT height)
	{
		UINT mipLevels = 1;
		while (width > kDDSMinMipDimension && height > kDDSMinMipDimension)
		{
			width = std::max<UINT>(1, width / 2);
			height = std::max<UINT>(1, height / 2);

			if (width < kDDSMinMipDimension || height < kDDSMinMipDimension)
				break;

			++mipLevels;
		}

		return mipLevels;
	}

	bool IsSupportedTextureSourceExtension(const fs::path& path)
	{
		const wstring extension = ToLowerCopy(path.extension().wstring());
		for (const wchar_t* supported : kSupportedTextureSourceExtensions)
		{
			if (extension == supported)
				return true;
		}

		return false;
	}

	wstring JoinTokens(const vector<wstring>& tokens, size_t beginIndex, size_t endIndex)
	{
		wstring joined;
		for (size_t i = beginIndex; i < endIndex; ++i)
		{
			if (!joined.empty())
				joined += L"_";

			joined += tokens[i];
		}

		return joined;
	}

	vector<fs::path> GetAssetSearchRoots(const wstring& defaultAssetPath)
	{
		vector<fs::path> roots;

		auto pushUnique = [&roots](const fs::path& candidate)
			{
				const wstring normalized = ToLowerCopy(candidate.generic_wstring());
				for (const fs::path& existing : roots)
				{
					if (ToLowerCopy(existing.generic_wstring()) == normalized)
						return;
				}

				roots.push_back(candidate);
			};

		pushUnique(fs::path(defaultAssetPath));
		pushUnique(fs::path(L"Assets"));
		pushUnique(fs::path(L"../Assets"));
		pushUnique(fs::path(L"Client/Assets"));
		pushUnique(fs::path(L"../Client/Assets"));

		return roots;
	}

	fs::path FindSourceTexturePathForDDS(const vector<fs::path>& assetRoots, const wstring& ddsStem)
	{
		const vector<wstring> tokens = CEngineString::Split(ddsStem, L"_");
		if (tokens.size() < 2)
			return {};

		for (size_t splitIndex = tokens.size() - 1; splitIndex > 0; --splitIndex)
		{
			const wstring folderName = ToLowerCopy(JoinTokens(tokens, 0, splitIndex));
			const wstring fileName = ToLowerCopy(JoinTokens(tokens, splitIndex, tokens.size()));
			if (folderName.empty() || fileName.empty())
				continue;

			for (const fs::path& assetRoot : assetRoots)
			{
				if (!fs::exists(assetRoot))
					continue;

				for (const auto& entry : fs::recursive_directory_iterator(assetRoot, fs::directory_options::skip_permission_denied))
				{
					if (!entry.is_regular_file())
						continue;

					const fs::path sourcePath = entry.path();
					if (!IsSupportedTextureSourceExtension(sourcePath))
						continue;
					if (ToLowerCopy(sourcePath.parent_path().filename().wstring()) != folderName)
						continue;
					if (ToLowerCopy(sourcePath.stem().wstring()) != fileName)
						continue;

					return sourcePath;
				}
			}
		}

		return {};
	}

	struct PrivateFontRegistration
	{
		wstring path;
		bool loaded = false;

		explicit PrivateFontRegistration(const wstring& fontPath)
			: path(fontPath)
		{
			loaded = (AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) > 0);
		}

		~PrivateFontRegistration()
		{
			if (loaded)
				RemoveFontResourceExW(path.c_str(), FR_PRIVATE, nullptr);
		}
	};

	uint16_t ReadBigEndianUInt16(const uint8_t* data)
	{
		return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) | data[1]);
	}

	uint32_t ReadBigEndianUInt32(const uint8_t* data)
	{
		return
			(static_cast<uint32_t>(data[0]) << 24) |
			(static_cast<uint32_t>(data[1]) << 16) |
			(static_cast<uint32_t>(data[2]) << 8) |
			static_cast<uint32_t>(data[3]);
	}

	bool TryDecodeUtf16BE(const uint8_t* data, size_t size, wstring& outValue)
	{
		if ((size % 2) != 0)
			return false;

		outValue.clear();
		outValue.reserve(size / 2);

		for (size_t i = 0; i < size; i += 2)
		{
			const wchar_t ch = static_cast<wchar_t>(ReadBigEndianUInt16(data + i));
			if (ch != L'\0')
				outValue.push_back(ch);
		}

		return !outValue.empty();
	}

	wstring ToLowerCopy(wstring value)
	{
		transform(value.begin(), value.end(), value.begin(), [](wchar_t ch)
			{
				return static_cast<wchar_t>(towlower(ch));
			});
		return value;
	}

	bool TryExtractFontName(const vector<uint8_t>& bytes, uint16_t desiredNameId, wstring& outValue)
	{
		if (bytes.size() < 12)
			return false;

		const uint16_t tableCount = ReadBigEndianUInt16(bytes.data() + 4);
		size_t nameTableOffset = 0;
		size_t nameTableLength = 0;

		for (uint16_t i = 0; i < tableCount; ++i)
		{
			const size_t entryOffset = 12ull + static_cast<size_t>(i) * 16ull;
			if (entryOffset + 16 > bytes.size())
				return false;

			const uint32_t tag = ReadBigEndianUInt32(bytes.data() + entryOffset);
			if (tag != 0x6E616D65) // 'name'
				continue;

			nameTableOffset = ReadBigEndianUInt32(bytes.data() + entryOffset + 8);
			nameTableLength = ReadBigEndianUInt32(bytes.data() + entryOffset + 12);
			break;
		}

		if (nameTableOffset == 0 || nameTableOffset + nameTableLength > bytes.size())
			return false;

		const uint8_t* nameTable = bytes.data() + nameTableOffset;
		const uint16_t recordCount = ReadBigEndianUInt16(nameTable + 2);
		const uint16_t stringStorageOffset = ReadBigEndianUInt16(nameTable + 4);
		const size_t recordsOffset = nameTableOffset + 6;
		const size_t stringsOffset = nameTableOffset + stringStorageOffset;

		int bestScore = -1;
		wstring bestValue;

		for (uint16_t i = 0; i < recordCount; ++i)
		{
			const size_t recordOffset = recordsOffset + static_cast<size_t>(i) * 12ull;
			if (recordOffset + 12 > bytes.size())
				return false;

			const uint16_t platformId = ReadBigEndianUInt16(bytes.data() + recordOffset + 0);
			const uint16_t encodingId = ReadBigEndianUInt16(bytes.data() + recordOffset + 2);
			const uint16_t languageId = ReadBigEndianUInt16(bytes.data() + recordOffset + 4);
			const uint16_t nameId = ReadBigEndianUInt16(bytes.data() + recordOffset + 6);
			const uint16_t stringLength = ReadBigEndianUInt16(bytes.data() + recordOffset + 8);
			const uint16_t stringOffset = ReadBigEndianUInt16(bytes.data() + recordOffset + 10);

			if (nameId != desiredNameId)
				continue;

			const size_t absoluteStringOffset = stringsOffset + stringOffset;
			if (absoluteStringOffset + stringLength > bytes.size())
				continue;

			if (platformId != 0 && platformId != 3)
				continue;

			wstring value;
			if (!TryDecodeUtf16BE(bytes.data() + absoluteStringOffset, stringLength, value))
				continue;

			int score = 0;
			if (platformId == 3)
				score += 100;
			if (platformId == 0)
				score += 90;
			if (languageId == 0x0409)
				score += 10;
			if (encodingId == 10 || encodingId == 1)
				score += 5;

			if (score > bestScore)
			{
				bestScore = score;
				bestValue = move(value);
			}
		}

		if (bestValue.empty())
			return false;

		outValue = move(bestValue);
		return true;
	}

	struct ScopedDeleteDC
	{
		HDC dc = nullptr;

		explicit ScopedDeleteDC(HDC handle)
			: dc(handle)
		{
		}

		~ScopedDeleteDC()
		{
			if (dc)
				DeleteDC(dc);
		}
	};

	struct ScopedDeleteObject
	{
		HGDIOBJ object = nullptr;

		explicit ScopedDeleteObject(HGDIOBJ handle)
			: object(handle)
		{
		}

		~ScopedDeleteObject()
		{
			if (object)
				DeleteObject(object);
		}
	};

	struct ScopedSelectObject
	{
		HDC dc = nullptr;
		HGDIOBJ previous = nullptr;

		ScopedSelectObject(HDC targetDc, HGDIOBJ object)
			: dc(targetDc)
		{
			if (dc)
				previous = SelectObject(dc, object);
		}

		~ScopedSelectObject()
		{
			if (dc && previous)
				SelectObject(dc, previous);
		}
	};

	struct SpriteFontGlyphData
	{
		wchar_t character = 0;
		RECT subrect = { 0, 0, 0, 0 };
		float xOffset = 0.f;
		float yOffset = 0.f;
		float xAdvance = 0.f;
		int width = 1;
		int height = 1;
		vector<uint8_t> alpha;
	};

	bool ResolveFontInputPath(const wstring& inputPath, wstring& outAbsolutePath)
	{
		if (inputPath.empty())
			return false;

		wchar_t exeDir[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
		PathRemoveFileSpecW(exeDir);

		fs::path candidate = fs::path(inputPath);
		if (!candidate.is_absolute())
			candidate = fs::path(exeDir) / candidate;

		error_code ec;
		const fs::path absolutePath = fs::absolute(candidate, ec);
		if (ec || !fs::exists(absolutePath))
			return false;

		outAbsolutePath = absolutePath.wstring();
		return true;
	}

	bool TryGetPrivateFontInfo(const wstring& fontPath, wstring& outFamilyName, LONG& outWeight, bool& outItalic)
	{
		ifstream in(fontPath, ios::binary);
		if (!in.is_open())
			return false;

		vector<uint8_t> bytes((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
		if (bytes.empty())
			return false;

		if (!TryExtractFontName(bytes, 16, outFamilyName) && !TryExtractFontName(bytes, 1, outFamilyName))
			return false;

		wstring styleName;
		TryExtractFontName(bytes, 17, styleName);
		if (styleName.empty())
			TryExtractFontName(bytes, 2, styleName);

		const wstring styleNameLower = ToLowerCopy(styleName);
		outWeight =
			(styleNameLower.find(L"bold") != wstring::npos ||
				styleNameLower.find(L"black") != wstring::npos ||
				styleNameLower.find(L"heavy") != wstring::npos)
			? FW_BOLD
			: FW_NORMAL;
		outItalic =
			(styleNameLower.find(L"italic") != wstring::npos ||
				styleNameLower.find(L"oblique") != wstring::npos);

		return true;
	}

	HFONT CreateSpriteFontHandle(HDC dc, const wstring& familyName, LONG fontWeight, bool italic, float pointSize)
	{
		if (!dc)
			return nullptr;

		LOGFONTW fontDesc = {};
		fontDesc.lfHeight = -MulDiv(static_cast<int>(pointSize), GetDeviceCaps(dc, LOGPIXELSY), 72);
		fontDesc.lfWeight = fontWeight;
		fontDesc.lfItalic = italic ? TRUE : FALSE;
		fontDesc.lfCharSet = DEFAULT_CHARSET;
		fontDesc.lfOutPrecision = OUT_TT_ONLY_PRECIS;
		fontDesc.lfQuality = ANTIALIASED_QUALITY;
		fontDesc.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
		wcsncpy_s(fontDesc.lfFaceName, familyName.c_str(), _TRUNCATE);

		return CreateFontIndirectW(&fontDesc);
	}

	bool BuildGlyphBitmap(HDC dc, wchar_t character, float ascent, SpriteFontGlyphData& outGlyph)
	{
		MAT2 identity =
		{
			{ 0, 1 }, { 0, 0 },
			{ 0, 0 }, { 0, 1 }
		};

		GLYPHMETRICS metrics = {};
		const DWORD bufferSize = GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP, &metrics, 0, nullptr, &identity);
		if (bufferSize == GDI_ERROR)
			return false;

		outGlyph.character = character;
		outGlyph.xAdvance = static_cast<float>(metrics.gmCellIncX);
		outGlyph.xOffset = static_cast<float>(metrics.gmptGlyphOrigin.x);
		outGlyph.yOffset = ascent - static_cast<float>(metrics.gmptGlyphOrigin.y);

		const int bitmapWidth = static_cast<int>(metrics.gmBlackBoxX);
		const int bitmapHeight = static_cast<int>(metrics.gmBlackBoxY);

		outGlyph.width = max(bitmapWidth, 1);
		outGlyph.height = max(bitmapHeight, 1);
		outGlyph.alpha.assign(static_cast<size_t>(outGlyph.width) * static_cast<size_t>(outGlyph.height), 0);

		if (bitmapWidth == 0 || bitmapHeight == 0 || bufferSize == 0)
			return true;

		vector<uint8_t> glyphBuffer(bufferSize);
		if (GetGlyphOutlineW(dc, character, GGO_GRAY8_BITMAP, &metrics, bufferSize, glyphBuffer.data(), &identity) == GDI_ERROR)
			return false;

		const int sourceStride = (bitmapWidth + 3) & ~3;
		for (int y = 0; y < bitmapHeight; ++y)
		{
			for (int x = 0; x < bitmapWidth; ++x)
			{
				const uint8_t coverage = glyphBuffer[static_cast<size_t>(y) * sourceStride + x];
				outGlyph.alpha[static_cast<size_t>(y) * outGlyph.width + x] =
					static_cast<uint8_t>(min(255, (static_cast<int>(coverage) * 255 + 32) / 64));
			}
		}

		return true;
	}

	bool PackGlyphs(vector<SpriteFontGlyphData>& glyphs, int& outTextureWidth, int& outTextureHeight)
	{
		for (int candidateWidth = 256; candidateWidth <= 2048; candidateWidth *= 2)
		{
			int cursorX = kSpriteFontPadding;
			int cursorY = kSpriteFontPadding;
			int rowHeight = 0;
			bool packed = true;

			for (auto& glyph : glyphs)
			{
				if (glyph.width + (kSpriteFontPadding * 2) > candidateWidth)
				{
					packed = false;
					break;
				}

				if (cursorX + glyph.width + kSpriteFontPadding > candidateWidth)
				{
					cursorX = kSpriteFontPadding;
					cursorY += rowHeight + kSpriteFontPadding;
					rowHeight = 0;
				}

				glyph.subrect.left = cursorX;
				glyph.subrect.top = cursorY;
				glyph.subrect.right = cursorX + glyph.width;
				glyph.subrect.bottom = cursorY + glyph.height;

				cursorX += glyph.width + kSpriteFontPadding;
				rowHeight = max(rowHeight, glyph.height);
			}

			if (!packed)
				continue;

			outTextureWidth = candidateWidth;
			outTextureHeight = max(cursorY + rowHeight + kSpriteFontPadding, 1);
			return true;
		}

		return false;
	}

	vector<uint8_t> BuildFontTexture(const vector<SpriteFontGlyphData>& glyphs, int textureWidth, int textureHeight)
	{
		vector<uint8_t> pixels(static_cast<size_t>(textureWidth) * static_cast<size_t>(textureHeight) * 4, 0);

		for (const auto& glyph : glyphs)
		{
			for (int y = 0; y < glyph.height; ++y)
			{
				for (int x = 0; x < glyph.width; ++x)
				{
					const uint8_t alpha = glyph.alpha[static_cast<size_t>(y) * glyph.width + x];
					if (alpha == 0)
						continue;

					const size_t pixelIndex =
						(static_cast<size_t>(glyph.subrect.top + y) * textureWidth + static_cast<size_t>(glyph.subrect.left + x)) * 4;

					pixels[pixelIndex + 0] = 255;
					pixels[pixelIndex + 1] = 255;
					pixels[pixelIndex + 2] = 255;
					pixels[pixelIndex + 3] = alpha;
				}
			}
		}

		return pixels;
	}

	bool SaveSpriteFontBinary(
		const wstring& outputPath,
		const vector<SpriteFontGlyphData>& glyphs,
		float lineSpacing,
		wchar_t defaultCharacter,
		int textureWidth,
		int textureHeight,
		const vector<uint8_t>& pixels)
	{
		ofstream out(outputPath, ios::binary);
		if (!out.is_open())
			return false;

		const string magic = "DXTKfont";
		out.write(magic.data(), magic.size());

		const int32_t glyphCount = static_cast<int32_t>(glyphs.size());
		out.write(reinterpret_cast<const char*>(&glyphCount), sizeof(glyphCount));

		for (const auto& glyph : glyphs)
		{
			const int32_t character = static_cast<int32_t>(glyph.character);
			const int32_t left = glyph.subrect.left;
			const int32_t top = glyph.subrect.top;
			const int32_t right = glyph.subrect.right;
			const int32_t bottom = glyph.subrect.bottom;

			out.write(reinterpret_cast<const char*>(&character), sizeof(character));
			out.write(reinterpret_cast<const char*>(&left), sizeof(left));
			out.write(reinterpret_cast<const char*>(&top), sizeof(top));
			out.write(reinterpret_cast<const char*>(&right), sizeof(right));
			out.write(reinterpret_cast<const char*>(&bottom), sizeof(bottom));
			out.write(reinterpret_cast<const char*>(&glyph.xOffset), sizeof(glyph.xOffset));
			out.write(reinterpret_cast<const char*>(&glyph.yOffset), sizeof(glyph.yOffset));
			out.write(reinterpret_cast<const char*>(&glyph.xAdvance), sizeof(glyph.xAdvance));
		}

		out.write(reinterpret_cast<const char*>(&lineSpacing), sizeof(lineSpacing));

		const int32_t defaultCharacterCode = static_cast<int32_t>(defaultCharacter);
		out.write(reinterpret_cast<const char*>(&defaultCharacterCode), sizeof(defaultCharacterCode));

		const int32_t width = textureWidth;
		const int32_t height = textureHeight;
		const int32_t format = static_cast<int32_t>(DXGI_FORMAT_R8G8B8A8_UNORM);
		const int32_t stride = textureWidth * 4;
		const int32_t rows = textureHeight;

		out.write(reinterpret_cast<const char*>(&width), sizeof(width));
		out.write(reinterpret_cast<const char*>(&height), sizeof(height));
		out.write(reinterpret_cast<const char*>(&format), sizeof(format));
		out.write(reinterpret_cast<const char*>(&stride), sizeof(stride));
		out.write(reinterpret_cast<const char*>(&rows), sizeof(rows));
		out.write(reinterpret_cast<const char*>(pixels.data()), static_cast<streamsize>(pixels.size()));

		return out.good();
	}

	void WriteBinaryWString(ofstream& out, const wstring& value)
	{
		_uint size = static_cast<_uint>(value.size());
		out.write(reinterpret_cast<const char*>(&size), sizeof(_uint));
		if (size > 0)
			out.write(reinterpret_cast<const char*>(value.data()), sizeof(wchar_t) * size);
	}

	wstring ReadBinaryWString(ifstream& in)
	{
		_uint size = 0;
		in.read(reinterpret_cast<char*>(&size), sizeof(_uint));
		if (size == 0)
			return L"";

		wstring value(size, L'\0');
		in.read(reinterpret_cast<char*>(&value[0]), sizeof(wchar_t) * size);
		return value;
	}

	void WriteSceneNavBakeOptions(ofstream& out, const EngineAI::CNaviMesh::NavBakeOptions& options)
	{
		out.write(reinterpret_cast<const char*>(&options.cellSize), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.cellHeight), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.agentHeight), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.agentRadius), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.agentMaxClimb), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.agentMaxSlope), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.regionMinSize), sizeof(_int));
		out.write(reinterpret_cast<const char*>(&options.regionMergeSize), sizeof(_int));
		out.write(reinterpret_cast<const char*>(&options.edgeMaxLen), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.edgeMaxError), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.vertsPerPoly), sizeof(_int));
		out.write(reinterpret_cast<const char*>(&options.detailSampleDist), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.detailSampleMaxError), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.queryHalfExtents.x), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.queryHalfExtents.y), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.queryHalfExtents.z), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&options.rasterizeObstacleMeshes), sizeof(_bool));
	}

	void ReadSceneNavBakeOptions(ifstream& in, EngineAI::CNaviMesh::NavBakeOptions& options)
	{
		in.read(reinterpret_cast<char*>(&options.cellSize), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.cellHeight), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.agentHeight), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.agentRadius), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.agentMaxClimb), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.agentMaxSlope), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.regionMinSize), sizeof(_int));
		in.read(reinterpret_cast<char*>(&options.regionMergeSize), sizeof(_int));
		in.read(reinterpret_cast<char*>(&options.edgeMaxLen), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.edgeMaxError), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.vertsPerPoly), sizeof(_int));
		in.read(reinterpret_cast<char*>(&options.detailSampleDist), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.detailSampleMaxError), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.queryHalfExtents.x), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.queryHalfExtents.y), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.queryHalfExtents.z), sizeof(_float));
		in.read(reinterpret_cast<char*>(&options.rasterizeObstacleMeshes), sizeof(_bool));
	}

	void WriteSceneSerializedNavMesh(ofstream& out, const EngineAI::CNaviMesh::SerializedNavMeshData& data)
	{
		const _bool hasBakedNavMesh = !data.tiles.empty();
		out.write(reinterpret_cast<const char*>(&hasBakedNavMesh), sizeof(_bool));
		if (!hasBakedNavMesh)
			return;

		out.write(reinterpret_cast<const char*>(&data.params), sizeof(dtNavMeshParams));

		const _uint tileCount = static_cast<_uint>(data.tiles.size());
		out.write(reinterpret_cast<const char*>(&tileCount), sizeof(_uint));

		for (const auto& tile : data.tiles)
		{
			out.write(reinterpret_cast<const char*>(&tile.tileRef), sizeof(uint64_t));

			const _uint tileDataSize = static_cast<_uint>(tile.data.size());
			out.write(reinterpret_cast<const char*>(&tileDataSize), sizeof(_uint));
			if (tileDataSize > 0)
				out.write(reinterpret_cast<const char*>(tile.data.data()), tileDataSize);
		}
	}

	void ReadSceneSerializedNavMesh(ifstream& in, const _uint version, EngineAI::CNaviMesh::SerializedNavMeshData& data)
	{
		data = {};

		if (version < 2)
			return;

		_bool hasBakedNavMesh = false;
		in.read(reinterpret_cast<char*>(&hasBakedNavMesh), sizeof(_bool));
		if (!in || !hasBakedNavMesh)
			return;

		in.read(reinterpret_cast<char*>(&data.params), sizeof(dtNavMeshParams));

		_uint tileCount = 0;
		in.read(reinterpret_cast<char*>(&tileCount), sizeof(_uint));
		if (!in)
			return;

		data.tiles.reserve(tileCount);
		for (_uint tileIndex = 0; tileIndex < tileCount; ++tileIndex)
		{
			EngineAI::CNaviMesh::SerializedTileData tile = {};
			in.read(reinterpret_cast<char*>(&tile.tileRef), sizeof(uint64_t));

			_uint tileDataSize = 0;
			in.read(reinterpret_cast<char*>(&tileDataSize), sizeof(_uint));
			if (!in)
				return;

			tile.data.resize(tileDataSize);
			if (tileDataSize > 0)
				in.read(reinterpret_cast<char*>(tile.data.data()), tileDataSize);
			if (!in)
				return;

			data.tiles.push_back(move(tile));
		}
	}
}

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
	if (!fs::exists("BinaryAssets/TextureData"))
		fs::create_directories("BinaryAssets/TextureData");

	CleanupOrphanedBinaries();

	Ready_GameResources();

	return S_OK;
}

void CResources::Release()
{
	for (TRAVERSAL_ITER(m_mGameResourceList, it))
		Safe_Release((*it).second);

	m_mGameResourceList.clear();
}

static _bool HasSourceAssetForBinary(const fs::path& binaryFile, const wstring& assetRoot, const vector<wstring>& sourceExts)
{
	const wstring stem = binaryFile.stem().wstring();

	for (size_t i = 0; i < stem.size(); ++i)
	{
		if (stem[i] != L'_')
			continue;

		const wstring folder = stem.substr(0, i);
		const wstring name = stem.substr(i + 1);

		if (folder.empty() || name.empty())
			continue;

		for (const wstring& ext : sourceExts)
		{
			// Search recursively under assetRoot for folder/name+ext
			error_code ec;
			for (const auto& dir : fs::recursive_directory_iterator(assetRoot, fs::directory_options::skip_permission_denied, ec))
			{
				if (!dir.is_directory())
					continue;

				if (dir.path().filename().wstring() == folder)
				{
					error_code ec2;
					if (fs::exists(dir.path() / (name + ext), ec2))
						return true;
				}
			}
		}
	}
	return false;
}

void CResources::CleanupOrphanedBinaries()
{
	const wstring assetRoot = m_strDefaultAssetPath;

	struct BinaryFolderInfo
	{
		string dir;
		vector<wstring> sourceExts;
	};

	const vector<BinaryFolderInfo> folders =
	{
		{ "BinaryAssets/MeshData",                 { L".fbx" } },
		{ "BinaryAssets/SkinnedMeshData",          { L".fbx" } },
		{ "BinaryAssets/AnimationClipData",        { L".fbx" } },
		{ "BinaryAssets/TextureData",              { L".png", L".jpg", L".jpeg", L".bmp", L".tga", L".tif", L".tiff" } },
		{ "BinaryAssets/AnimatorControllerData",   { L".animatorcontroller" } },
	};

	for (const auto& folder : folders)
	{
		error_code ec;
		if (!fs::exists(folder.dir, ec))
			continue;

		for (const auto& entry : fs::directory_iterator(folder.dir, ec))
		{
			if (!entry.is_regular_file())
				continue;

			if (!HasSourceAssetForBinary(entry.path(), assetRoot, folder.sourceExts))
			{
				CDebug::Log(L"Cleanup orphaned binary: " + entry.path().wstring());
				error_code removeEc;
				fs::remove(entry.path(), removeEc);
			}
		}
	}
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

	struct StaticMeshNodeInstance
	{
		const aiMesh* mesh = nullptr;
		aiNode* node = nullptr;
		aiMatrix4x4 globalTransform = aiMatrix4x4();
		_uint meshIndex = 0u;
		_uint nodeMeshSlot = 0u;
	};

	vector<StaticMeshNodeInstance> meshInstances = {};
	meshInstances.reserve(aiScene->mNumMeshes);

	function<void(aiNode*, const aiMatrix4x4&)> CollectMeshInstances =
		[&](aiNode* node, const aiMatrix4x4& parentTrafo)
		{
			if (!node)
				return;

			const aiMatrix4x4 current = parentTrafo * node->mTransformation;
			for (_uint mi = 0; mi < node->mNumMeshes; ++mi)
			{
				const _uint meshIndex = node->mMeshes[mi];
				if (meshIndex >= aiScene->mNumMeshes)
					continue;

				StaticMeshNodeInstance instance = {};
				instance.mesh = aiScene->mMeshes[meshIndex];
				instance.node = node;
				instance.globalTransform = current;
				instance.meshIndex = meshIndex;
				instance.nodeMeshSlot = mi;
				meshInstances.push_back(instance);
			}

			for (_uint ci = 0; ci < node->mNumChildren; ++ci)
				CollectMeshInstances(node->mChildren[ci], current);
		};
	CollectMeshInstances(aiScene->mRootNode, aiMatrix4x4());

	vector<CMeshBuffer::MeshBufferInitiaizeInfo> bufferInfoList;
	bufferInfoList.reserve(meshInstances.size());
	unordered_map<wstring, _uint> meshNameCounts = {};

	for (const StaticMeshNodeInstance& instance : meshInstances)
	{
		const aiMesh* mesh = instance.mesh;
		if (!mesh)
			continue;

		const aiMatrix4x4& gMat = instance.globalTransform;
		aiMatrix3x3 gMat3 = aiMatrix3x3(gMat).Inverse().Transpose();

		CMeshBuffer::MeshBufferInitiaizeInfo info{};
		info.sourceAssetPath = CEngineString::Replace(_filePath, L"\\", L"/");

		wstring baseMeshName = {};
		if (instance.node && instance.node->mName.length > 0)
			baseMeshName = CEngineString::StringToWString(instance.node->mName.C_Str());
		if (baseMeshName.empty() && mesh->mName.length > 0)
			baseMeshName = CEngineString::StringToWString(mesh->mName.C_Str());
		if (baseMeshName.empty())
			baseMeshName = L"Mesh_" + to_wstring(instance.meshIndex);
		if (instance.node && instance.node->mNumMeshes > 1)
			baseMeshName += L"_" + to_wstring(instance.nodeMeshSlot);

		_uint& meshNameCount = meshNameCounts[baseMeshName];
		info.meshName = (meshNameCount == 0u) ? baseMeshName : (baseMeshName + L"_" + to_wstring(meshNameCount));
		++meshNameCount;

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
		if (!vertices.empty())
		{
			_float3 minPoint = vertices[0].position;
			_float3 maxPoint = vertices[0].position;

			for (const auto& vertex : vertices)
			{
				minPoint.x = min(minPoint.x, vertex.position.x);
				minPoint.y = min(minPoint.y, vertex.position.y);
				minPoint.z = min(minPoint.z, vertex.position.z);

				maxPoint.x = max(maxPoint.x, vertex.position.x);
				maxPoint.y = max(maxPoint.y, vertex.position.y);
				maxPoint.z = max(maxPoint.z, vertex.position.z);
			}

			desc.boundingBox.Center =
			{
				(minPoint.x + maxPoint.x) * 0.5f,
				(minPoint.y + maxPoint.y) * 0.5f,
				(minPoint.z + maxPoint.z) * 0.5f
			};

			desc.boundingBox.Extents =
			{
				(maxPoint.x - minPoint.x) * 0.5f,
				(maxPoint.y - minPoint.y) * 0.5f,
				(maxPoint.z - minPoint.z) * 0.5f
			};
		}

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

HRESULT CResources::ConvertImageToDDS(const wstring _filePath)
{
	ID3D11Device* device = CGraphicDevice::GetInstance().Get_Device();
	ID3D11DeviceContext* context = CGraphicDevice::GetInstance().Get_Context();

	if (!device || !context)
	{
		CDebug::LogError(L"Failed create texture Data - Invalid device or context: " + _filePath);
		return E_FAIL;
	}

	ComPtr<ID3D11Resource> sourceResource;
	if (FAILED(CreateWICTextureFromFile(device, _filePath.c_str(), sourceResource.GetAddressOf(), nullptr, kDDSMaxTextureSize)))
	{
		CDebug::LogError(L"Failed create texture Data - Can not load image: " + _filePath);
		return E_FAIL;
	}

	if (!sourceResource)
	{
		CDebug::LogError(L"Failed create texture Data - Invalid source resource: " + _filePath);
		return E_FAIL;
	}

	ComPtr<ID3D11Texture2D> sourceTexture;
	if (FAILED(sourceResource.As(&sourceTexture)) || !sourceTexture)
	{
		CDebug::LogError(L"Failed create texture Data - Invalid source texture: " + _filePath);
		return E_FAIL;
	}

	D3D11_TEXTURE2D_DESC sourceDesc{};
	sourceTexture->GetDesc(&sourceDesc);

	ID3D11Resource* saveResource = sourceResource.Get();
	ComPtr<ID3D11Texture2D> mipTexture;
	ComPtr<ID3D11ShaderResourceView> mipSRV;

	const UINT mipLevels = CalculateDDSMipLevels(sourceDesc.Width, sourceDesc.Height);
	if (mipLevels > 1)
	{
		UINT formatSupport = 0;
		if (FAILED(device->CheckFormatSupport(sourceDesc.Format, &formatSupport)) ||
			(formatSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) == 0)
		{
			CDebug::LogWarnning(L"Texture Data - Mip autogen unsupported, saving single mip: " + _filePath);
		}
		else
		{
			D3D11_TEXTURE2D_DESC mipDesc = sourceDesc;
			mipDesc.MipLevels = mipLevels;
			mipDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
			mipDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

			if (FAILED(device->CreateTexture2D(&mipDesc, nullptr, mipTexture.GetAddressOf())))
			{
				CDebug::LogError(L"Failed create texture Data - Can not create mip texture: " + _filePath);
				return E_FAIL;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = mipDesc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = mipLevels;

			if (FAILED(device->CreateShaderResourceView(mipTexture.Get(), &srvDesc, mipSRV.GetAddressOf())))
			{
				CDebug::LogError(L"Failed create texture Data - Can not create mip SRV: " + _filePath);
				return E_FAIL;
			}

			context->CopySubresourceRegion(mipTexture.Get(), 0, 0, 0, 0, sourceTexture.Get(), 0, nullptr);
			context->GenerateMips(mipSRV.Get());
			saveResource = mipTexture.Get();
		}
	}

	auto pathSplit = CEngineString::Split(CEngineString::Replace(_filePath, L"\\", L"/"), L"/");
	if (pathSplit.size() < 2)
	{
		CDebug::LogError(L"Failed create texture Data - Invalid path: " + _filePath);
		return E_FAIL;
	}

	const wstring folder = pathSplit[pathSplit.size() - 2];
	const wstring fileNoExt = CEngineString::Split(pathSplit.back(), L".")[0];
	const wstring savePath = L"BinaryAssets/TextureData/" + folder + L"_" + fileNoExt + L".dds";

	if (!fs::exists("BinaryAssets/TextureData"))
		fs::create_directories("BinaryAssets/TextureData");

	if (FAILED(DirectX::SaveDDSTextureToFile(context, saveResource, savePath.c_str())))
	{
		CDebug::LogError(L"Failed create texture Data - Can not save DDS: " + _filePath);
		return E_FAIL;
	}

	CDebug::Log(L"Complete create texture Data: " + _filePath);
	return S_OK;
}

HRESULT CResources::RebuildDDSFromBinaryPath(const wstring _ddsPath)
{
	const fs::path ddsPath = fs::path(CEngineString::Replace(_ddsPath, L"\\", L"/"));
	if (ToLowerCopy(ddsPath.extension().wstring()) != L".dds")
		return E_INVALIDARG;

	const vector<fs::path> assetRoots = GetAssetSearchRoots(m_strDefaultAssetPath);
	const fs::path sourcePath = FindSourceTexturePathForDDS(assetRoots, ddsPath.stem().wstring());
	if (sourcePath.empty())
	{
		CDebug::LogWarnning(L"Texture load recovery failed - source image not found: " + _ddsPath);
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	const HRESULT result = ConvertImageToDDS(sourcePath.generic_wstring());
	if (FAILED(result))
	{
		CDebug::LogWarnning(L"Texture load recovery failed - DDS rebuild failed: " + _ddsPath);
		return result;
	}

	CDebug::LogWarnning(L"Texture load recovery rebuilt DDS: " + _ddsPath);
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
		info.sourceAssetPath = CEngineString::Replace(_filePath, L"\\", L"/");

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
		if (!vertices.empty())
		{
			_float3 minPoint = vertices[0].position;
			_float3 maxPoint = vertices[0].position;

			for (const auto& vertex : vertices)
			{
				minPoint.x = min(minPoint.x, vertex.position.x);
				minPoint.y = min(minPoint.y, vertex.position.y);
				minPoint.z = min(minPoint.z, vertex.position.z);

				maxPoint.x = max(maxPoint.x, vertex.position.x);
				maxPoint.y = max(maxPoint.y, vertex.position.y);
				maxPoint.z = max(maxPoint.z, vertex.position.z);
			}

			desc.boundingBox.Center =
			{
				(minPoint.x + maxPoint.x) * 0.5f,
				(minPoint.y + maxPoint.y) * 0.5f,
				(minPoint.z + maxPoint.z) * 0.5f
			};

			desc.boundingBox.Extents =
			{
				(maxPoint.x - minPoint.x) * 0.5f,
				(maxPoint.y - minPoint.y) * 0.5f,
				(maxPoint.z - minPoint.z) * 0.5f
			};
		}

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
	wstring absoluteFontPath;
	if (!ResolveFontInputPath(_filePath, absoluteFontPath))
	{
		CDebug::LogError(L"Invalid font path: " + _filePath);
		return E_FAIL;
	}

	wstring familyName;
	LONG fontWeight = FW_NORMAL;
	bool italic = false;
	if (!TryGetPrivateFontInfo(absoluteFontPath, familyName, fontWeight, italic))
	{
		CDebug::LogError(L"Failed to inspect font file: " + absoluteFontPath);
		return E_FAIL;
	}

	PrivateFontRegistration privateFont(absoluteFontPath);
	if (!privateFont.loaded)
	{
		CDebug::LogError(L"Failed to load font into the current process: " + absoluteFontPath);
		return E_FAIL;
	}

	ScopedDeleteDC dc(CreateCompatibleDC(nullptr));
	if (!dc.dc)
	{
		CDebug::LogError("CreateCompatibleDC failed while building spritefont.");
		return E_FAIL;
	}

	ScopedDeleteObject font(CreateSpriteFontHandle(dc.dc, familyName, fontWeight, italic, kSpriteFontPointSize));
	if (!font.object)
	{
		CDebug::LogError(L"CreateFontIndirect failed for font family: " + familyName);
		return E_FAIL;
	}

	ScopedSelectObject selectFont(dc.dc, font.object);

	TEXTMETRICW textMetric = {};
	if (!GetTextMetricsW(dc.dc, &textMetric))
	{
		CDebug::LogError(L"GetTextMetrics failed for font family: " + familyName);
		return E_FAIL;
	}

	vector<SpriteFontGlyphData> glyphs;
	glyphs.reserve(kSpriteFontLastChar - kSpriteFontFirstChar + 1);

	for (int ch = kSpriteFontFirstChar; ch <= kSpriteFontLastChar; ++ch)
	{
		SpriteFontGlyphData glyph;
		if (!BuildGlyphBitmap(dc.dc, static_cast<wchar_t>(ch), static_cast<float>(textMetric.tmAscent), glyph))
		{
			CDebug::LogError(L"Failed to rasterize glyph for character code: " + to_wstring(ch));
			return E_FAIL;
		}

		glyphs.push_back(move(glyph));
	}

	int textureWidth = 0;
	int textureHeight = 0;
	if (!PackGlyphs(glyphs, textureWidth, textureHeight))
	{
		CDebug::LogError(L"Failed to pack glyph atlas for font: " + familyName);
		return E_FAIL;
	}

	const vector<uint8_t> pixels = BuildFontTexture(glyphs, textureWidth, textureHeight);

	const fs::path outputDir = fs::path(L"BinaryAssets/FontData");
	error_code createDirError;
	fs::create_directories(outputDir, createDirError);
	if (createDirError)
	{
		CDebug::LogError(L"Failed to create font output directory: " + outputDir.wstring());
		return E_FAIL;
	}

	const fs::path outputPath = outputDir / (fs::path(absoluteFontPath).stem().wstring() + L".spritefont");
	if (!SaveSpriteFontBinary(
		outputPath.wstring(),
		glyphs,
		static_cast<float>(textMetric.tmHeight),
		L'?',
		textureWidth,
		textureHeight,
		pixels))
	{
		CDebug::LogError(L"Failed to save spritefont file: " + outputPath.wstring());
		return E_FAIL;
	}

	CDebug::Log(L"SpriteFont successfully created at: " + outputPath.wstring());
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
	const _uint version = 25;
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

		WriteBinaryWString(out, info.objTag);

		_uint pathSize = static_cast<_uint>(info.objPath.size());
		out.write(reinterpret_cast<const char*>(&pathSize), sizeof(_uint));
		if (pathSize > 0)
			out.write(reinterpret_cast<const char*>(info.objPath.data()), sizeof(wchar_t) * pathSize);
		out.write(reinterpret_cast<const char*>(&info.sceneOrder), sizeof(_int));
		out.write(reinterpret_cast<const char*>(&info.siblingIndex), sizeof(_int));

		out.write(reinterpret_cast<const char*>(&info.localPos), sizeof(_float3));
		out.write(reinterpret_cast<const char*>(&info.localQuaternion), sizeof(_float4));
		out.write(reinterpret_cast<const char*>(&info.localScale), sizeof(_float3));
		out.write(reinterpret_cast<const char*>(&info.isActive), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.objLayer), sizeof(_uint));
		out.write(reinterpret_cast<const char*>(&info.isTransformStatic), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.isNavigationStatic), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.isNavigationObstacleStatic), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyKinematic), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyUseGravity), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyMass), sizeof(_float));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstPositionX), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstPositionY), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstPositionZ), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstRotationX), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstRotationY), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&info.rigidBodyConstRotationZ), sizeof(_bool));

		out.write(reinterpret_cast<const char*>(&info.isRect), sizeof(_bool));
		if (info.isRect)
		{
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchoredPos), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.widthHeight), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.pivot), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchorMin), sizeof(_float2));
			out.write(reinterpret_cast<const char*>(&info.rectInfo.anchorMax), sizeof(_float2));
		}

		_uint componentCount = static_cast<_uint>(info.componentNames.size());
		out.write(reinterpret_cast<const char*>(&componentCount), sizeof(_uint));
		for (const auto& componentName : info.componentNames)
		{
			_uint componentNameSize = static_cast<_uint>(componentName.size());
			out.write(reinterpret_cast<const char*>(&componentNameSize), sizeof(_uint));
			if (componentNameSize > 0)
				out.write(reinterpret_cast<const char*>(componentName.data()), sizeof(wchar_t) * componentNameSize);
		}

		_uint componentEnabledCount = componentCount;
		out.write(reinterpret_cast<const char*>(&componentEnabledCount), sizeof(_uint));
		for (_uint componentIndex = 0; componentIndex < componentEnabledCount; ++componentIndex)
		{
			const _bool enabledState =
				componentIndex < info.componentEnabledStates.size() ? info.componentEnabledStates[componentIndex] : true;
			out.write(reinterpret_cast<const char*>(&enabledState), sizeof(_bool));
		}

		_uint lodSwitchDistanceCount = static_cast<_uint>(info.lodSwitchDistances.size());
		out.write(reinterpret_cast<const char*>(&lodSwitchDistanceCount), sizeof(_uint));
		for (const _float lodSwitchDistance : info.lodSwitchDistances)
			out.write(reinterpret_cast<const char*>(&lodSwitchDistance), sizeof(_float));

		_uint meshBufferNameSize = static_cast<_uint>(info.meshBufferName.size());
		out.write(reinterpret_cast<const char*>(&meshBufferNameSize), sizeof(_uint));
		if (meshBufferNameSize > 0)
			out.write(reinterpret_cast<const char*>(info.meshBufferName.data()), sizeof(wchar_t) * meshBufferNameSize);

		_uint materialNameSize = static_cast<_uint>(info.materialName.size());
		out.write(reinterpret_cast<const char*>(&materialNameSize), sizeof(_uint));
		if (materialNameSize > 0)
			out.write(reinterpret_cast<const char*>(info.materialName.data()), sizeof(wchar_t) * materialNameSize);

		out.write(reinterpret_cast<const char*>(&info.hasMaterialBaseColor), sizeof(_bool));
		if (info.hasMaterialBaseColor)
			out.write(reinterpret_cast<const char*>(&info.materialBaseColor), sizeof(_float4));

		_uint materialTextureCount = static_cast<_uint>(info.materialTextures.size());
		out.write(reinterpret_cast<const char*>(&materialTextureCount), sizeof(_uint));
		for (const auto& textureInfo : info.materialTextures)
		{
			_uint textureNameSize = static_cast<_uint>(textureInfo.name.size());
			out.write(reinterpret_cast<const char*>(&textureNameSize), sizeof(_uint));
			if (textureNameSize > 0)
				out.write(reinterpret_cast<const char*>(textureInfo.name.data()), sizeof(wchar_t) * textureNameSize);

			_uint texturePathSize = static_cast<_uint>(textureInfo.path.size());
			out.write(reinterpret_cast<const char*>(&texturePathSize), sizeof(_uint));
			if (texturePathSize > 0)
				out.write(reinterpret_cast<const char*>(textureInfo.path.data()), sizeof(wchar_t) * texturePathSize);
		}

		auto writeMaterialValueList = [&](const auto& valueList)
		{
			_uint valueCount = static_cast<_uint>(valueList.size());
			out.write(reinterpret_cast<const char*>(&valueCount), sizeof(_uint));
			for (const auto& [key, value] : valueList)
			{
				_uint keySize = static_cast<_uint>(key.size());
				out.write(reinterpret_cast<const char*>(&keySize), sizeof(_uint));
				if (keySize > 0)
					out.write(reinterpret_cast<const char*>(key.data()), sizeof(wchar_t) * keySize);
				out.write(reinterpret_cast<const char*>(&value), sizeof(value));
			}
		};

		writeMaterialValueList(info.materialFloatValues);
		writeMaterialValueList(info.materialIntValues);
		writeMaterialValueList(info.materialVector2Values);
		writeMaterialValueList(info.materialVector3Values);
		writeMaterialValueList(info.materialVector4Values);
		writeMaterialValueList(info.materialMatrixValues);

        out.write(reinterpret_cast<const char*>(&info.hasNaviMeshAgent), sizeof(_bool));
        if (info.hasNaviMeshAgent)
        {
            WriteBinaryWString(out, info.navAgentNavigationMeshResourceName);
            out.write(reinterpret_cast<const char*>(&info.navAgentMoveSpeed), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentAngularSpeed), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentStoppingDistance), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentWaypointTolerance), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentRadius), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentHeight), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentCenter), sizeof(_float3));
            out.write(reinterpret_cast<const char*>(&info.navAgentGroundSnapOffset), sizeof(_float));
            out.write(reinterpret_cast<const char*>(&info.navAgentCollisionWeight), sizeof(_int));
        }

		out.write(reinterpret_cast<const char*>(&info.lightInfo.hasLight), sizeof(_bool));
		if (info.lightInfo.hasLight)
		{
			out.write(reinterpret_cast<const char*>(&info.lightInfo.type), sizeof(_uint));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.intensity), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.range), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.spotAngle), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.attenuation), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.diffuseColor), sizeof(ColorValue));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.specularColor), sizeof(ColorValue));
			out.write(reinterpret_cast<const char*>(&info.lightInfo.castShadow), sizeof(_bool));
		}

		out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.hasHorizontalLayoutGroup), sizeof(_bool));
		if (info.horizontalLayoutGroupInfo.hasHorizontalLayoutGroup)
		{
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.paddingLeft), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.paddingRight), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.paddingTop), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.paddingBottom), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.spacing), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.childAlignment), sizeof(_int));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.controlChildSizeWidth), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.controlChildSizeHeight), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.forceExpandWidth), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.horizontalLayoutGroupInfo.forceExpandHeight), sizeof(_bool));
		}

		out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.hasVerticalLayoutGroup), sizeof(_bool));
		if (info.verticalLayoutGroupInfo.hasVerticalLayoutGroup)
		{
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.paddingLeft), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.paddingRight), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.paddingTop), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.paddingBottom), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.spacing), sizeof(_float));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.childAlignment), sizeof(_int));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.controlChildSizeWidth), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.controlChildSizeHeight), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.forceExpandWidth), sizeof(_bool));
			out.write(reinterpret_cast<const char*>(&info.verticalLayoutGroupInfo.forceExpandHeight), sizeof(_bool));
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

		if (version >= 16)
			info.objTag = ReadBinaryWString(in);
		else
			info.objTag = L"";

		if (version >= 3)
		{
			_uint pathSize = 0;
			in.read(reinterpret_cast<char*>(&pathSize), sizeof(_uint));
			if (pathSize > 0)
			{
				wstring temp(pathSize, L'\0');
				in.read(reinterpret_cast<char*>(&temp[0]), sizeof(wchar_t) * pathSize);
				info.objPath = move(temp);
			}
			else
			{
				info.objPath = L"";
			}
		}

		if (version >= 17)
		{
			in.read(reinterpret_cast<char*>(&info.sceneOrder), sizeof(_int));
			in.read(reinterpret_cast<char*>(&info.siblingIndex), sizeof(_int));
		}
		else
		{
			info.sceneOrder = static_cast<_int>(resultInfo.size());
			info.siblingIndex = -1;
		}

		_float3 pos = {};
		in.read(reinterpret_cast<char*>(&info.localPos), sizeof(_float3));
		in.read(reinterpret_cast<char*>(&info.localQuaternion), sizeof(_float4));
		in.read(reinterpret_cast<char*>(&info.localScale), sizeof(_float3));

		info.isActive = true;
		if (version >= 7)
			in.read(reinterpret_cast<char*>(&info.isActive), sizeof(_bool));

		if (version >= 11)
			in.read(reinterpret_cast<char*>(&info.objLayer), sizeof(_uint));

		if (version >= 10)
			in.read(reinterpret_cast<char*>(&info.isTransformStatic), sizeof(_bool));

		if (version >= 12)
			in.read(reinterpret_cast<char*>(&info.isNavigationStatic), sizeof(_bool));

		if (version >= 22)
			in.read(reinterpret_cast<char*>(&info.isNavigationObstacleStatic), sizeof(_bool));

		if (version >= 8)
		{
			in.read(reinterpret_cast<char*>(&info.rigidBodyKinematic), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyUseGravity), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyMass), sizeof(_float));
		}

		if (version >= 9)
		{
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstPositionX), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstPositionY), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstPositionZ), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstRotationX), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstRotationY), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&info.rigidBodyConstRotationZ), sizeof(_bool));
		}

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

		if (version >= 4)
		{
			_uint componentCount = 0;
			in.read(reinterpret_cast<char*>(&componentCount), sizeof(_uint));
			info.componentNames.reserve(componentCount);
			for (_uint c = 0; c < componentCount; ++c)
			{
				_uint componentNameSize = 0;
				in.read(reinterpret_cast<char*>(&componentNameSize), sizeof(_uint));
				if (componentNameSize > 0)
				{
					wstring componentName(componentNameSize, L'\0');
					in.read(reinterpret_cast<char*>(&componentName[0]), sizeof(wchar_t) * componentNameSize);
					info.componentNames.push_back(move(componentName));
				}
				else
					info.componentNames.push_back(L"");
			}
		}

		if (version >= 20)
		{
			_uint componentEnabledCount = 0;
			in.read(reinterpret_cast<char*>(&componentEnabledCount), sizeof(_uint));
			info.componentEnabledStates.reserve(componentEnabledCount);
			for (_uint componentIndex = 0; componentIndex < componentEnabledCount; ++componentIndex)
			{
				_bool enabledState = true;
				in.read(reinterpret_cast<char*>(&enabledState), sizeof(_bool));
				info.componentEnabledStates.push_back(enabledState);
			}
		}
		else
		{
			info.componentEnabledStates.assign(info.componentNames.size(), true);
		}

		if (version >= 21)
		{
			_uint lodSwitchDistanceCount = 0;
			in.read(reinterpret_cast<char*>(&lodSwitchDistanceCount), sizeof(_uint));
			info.lodSwitchDistances.reserve(lodSwitchDistanceCount);
			for (_uint lodIndex = 0; lodIndex < lodSwitchDistanceCount; ++lodIndex)
			{
				_float lodSwitchDistance = 0.f;
				in.read(reinterpret_cast<char*>(&lodSwitchDistance), sizeof(_float));
				info.lodSwitchDistances.push_back(lodSwitchDistance);
			}
		}

		if (version >= 5)
		{
			_uint meshBufferNameSize = 0;
			in.read(reinterpret_cast<char*>(&meshBufferNameSize), sizeof(_uint));
			if (meshBufferNameSize > 0)
			{
				wstring meshBufferName(meshBufferNameSize, L'\0');
				in.read(reinterpret_cast<char*>(&meshBufferName[0]), sizeof(wchar_t) * meshBufferNameSize);
				info.meshBufferName = move(meshBufferName);
			}

			_uint materialNameSize = 0;
			in.read(reinterpret_cast<char*>(&materialNameSize), sizeof(_uint));
			if (materialNameSize > 0)
			{
				wstring materialName(materialNameSize, L'\0');
				in.read(reinterpret_cast<char*>(&materialName[0]), sizeof(wchar_t) * materialNameSize);
				info.materialName = move(materialName);
			}

			if (version >= 25)
			{
				in.read(reinterpret_cast<char*>(&info.hasMaterialBaseColor), sizeof(_bool));
				if (info.hasMaterialBaseColor)
					in.read(reinterpret_cast<char*>(&info.materialBaseColor), sizeof(_float4));
			}
		}

		if (version >= 6)
		{
			_uint materialTextureCount = 0;
			in.read(reinterpret_cast<char*>(&materialTextureCount), sizeof(_uint));
			info.materialTextures.reserve(materialTextureCount);
			for (_uint textureIndex = 0; textureIndex < materialTextureCount; ++textureIndex)
			{
				CScene::ObjectsTransformInfo::MATERIALTEXTUREINFO textureInfo = {};

				_uint textureNameSize = 0;
				in.read(reinterpret_cast<char*>(&textureNameSize), sizeof(_uint));
				if (textureNameSize > 0)
				{
					wstring textureName(textureNameSize, L'\0');
					in.read(reinterpret_cast<char*>(&textureName[0]), sizeof(wchar_t) * textureNameSize);
					textureInfo.name = move(textureName);
				}

				_uint texturePathSize = 0;
				in.read(reinterpret_cast<char*>(&texturePathSize), sizeof(_uint));
				if (texturePathSize > 0)
				{
					wstring texturePath(texturePathSize, L'\0');
					in.read(reinterpret_cast<char*>(&texturePath[0]), sizeof(wchar_t) * texturePathSize);
					textureInfo.path = move(texturePath);
				}

				info.materialTextures.push_back(move(textureInfo));
			}

			auto readMaterialValueList = [&](auto& valueList)
			{
				_uint valueCount = 0;
				in.read(reinterpret_cast<char*>(&valueCount), sizeof(_uint));
				valueList.reserve(valueCount);
				for (_uint valueIndex = 0; valueIndex < valueCount; ++valueIndex)
				{
					_uint keySize = 0;
					in.read(reinterpret_cast<char*>(&keySize), sizeof(_uint));
					wstring key = L"";
					if (keySize > 0)
					{
						key.resize(keySize);
						in.read(reinterpret_cast<char*>(&key[0]), sizeof(wchar_t) * keySize);
					}

					typename decay_t<decltype(valueList)>::value_type::second_type value = {};
					in.read(reinterpret_cast<char*>(&value), sizeof(value));
					valueList.push_back({ move(key), value });
				}
			};

			readMaterialValueList(info.materialFloatValues);
			readMaterialValueList(info.materialIntValues);
			readMaterialValueList(info.materialVector2Values);
			readMaterialValueList(info.materialVector3Values);
			readMaterialValueList(info.materialVector4Values);
			readMaterialValueList(info.materialMatrixValues);

        if (version >= 13)
        {
            in.read(reinterpret_cast<char*>(&info.hasNaviMeshAgent), sizeof(_bool));
            if (info.hasNaviMeshAgent)
            {
                info.navAgentNavigationMeshResourceName = ReadBinaryWString(in);
                in.read(reinterpret_cast<char*>(&info.navAgentMoveSpeed), sizeof(_float));
                if (version >= 15)
                    in.read(reinterpret_cast<char*>(&info.navAgentAngularSpeed), sizeof(_float));
                else
                    info.navAgentAngularSpeed = 720.f;
                in.read(reinterpret_cast<char*>(&info.navAgentStoppingDistance), sizeof(_float));
                in.read(reinterpret_cast<char*>(&info.navAgentWaypointTolerance), sizeof(_float));
                in.read(reinterpret_cast<char*>(&info.navAgentRadius), sizeof(_float));
                in.read(reinterpret_cast<char*>(&info.navAgentHeight), sizeof(_float));
                if (version >= 14)
                    in.read(reinterpret_cast<char*>(&info.navAgentCenter), sizeof(_float3));
                else
                    info.navAgentCenter = _float3(0.f, info.navAgentHeight * 0.5f, 0.f);
                in.read(reinterpret_cast<char*>(&info.navAgentGroundSnapOffset), sizeof(_float));
                if (version >= 23)
                    in.read(reinterpret_cast<char*>(&info.navAgentCollisionWeight), sizeof(_int));
                else
                    info.navAgentCollisionWeight = 1;
            }
        }

		if (version >= 24)
		{
			in.read(reinterpret_cast<char*>(&info.lightInfo.hasLight), sizeof(_bool));
			if (info.lightInfo.hasLight)
			{
				in.read(reinterpret_cast<char*>(&info.lightInfo.type), sizeof(_uint));
				in.read(reinterpret_cast<char*>(&info.lightInfo.intensity), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.lightInfo.range), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.lightInfo.spotAngle), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.lightInfo.attenuation), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.lightInfo.diffuseColor), sizeof(ColorValue));
				in.read(reinterpret_cast<char*>(&info.lightInfo.specularColor), sizeof(ColorValue));
				in.read(reinterpret_cast<char*>(&info.lightInfo.castShadow), sizeof(_bool));
			}
		}

		if (version >= 18)
		{
			in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.hasHorizontalLayoutGroup), sizeof(_bool));
			if (info.horizontalLayoutGroupInfo.hasHorizontalLayoutGroup)
			{
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.paddingLeft), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.paddingRight), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.paddingTop), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.paddingBottom), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.spacing), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.childAlignment), sizeof(_int));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.controlChildSizeWidth), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.controlChildSizeHeight), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.forceExpandWidth), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.horizontalLayoutGroupInfo.forceExpandHeight), sizeof(_bool));
			}
		}

		if (version >= 19)
		{
			in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.hasVerticalLayoutGroup), sizeof(_bool));
			if (info.verticalLayoutGroupInfo.hasVerticalLayoutGroup)
			{
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.paddingLeft), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.paddingRight), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.paddingTop), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.paddingBottom), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.spacing), sizeof(_float));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.childAlignment), sizeof(_int));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.controlChildSizeWidth), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.controlChildSizeHeight), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.forceExpandWidth), sizeof(_bool));
				in.read(reinterpret_cast<char*>(&info.verticalLayoutGroupInfo.forceExpandHeight), sizeof(_bool));
			}
		}
		}

		resultInfo.push_back(info);
	}

	in.close();

	return resultInfo;
}

HRESULT CResources::SaveSceneNavigationInfos(const wstring _filePath, const vector<CScene::SCENENAVIGATIONINFO>& _infoList)
{
	using namespace std;

	ofstream out(_filePath, ios::binary);
	if (!out.is_open())
	{
		CDebug::LogError(L"SaveSceneNavigationInfos failed - can not open: " + _filePath);
		return E_FAIL;
	}

	const _uint magic = 0x4E415631;
	const _uint version = 2;
	const _uint count = static_cast<_uint>(_infoList.size());
	out.write(reinterpret_cast<const char*>(&magic), sizeof(_uint));
	out.write(reinterpret_cast<const char*>(&version), sizeof(_uint));
	out.write(reinterpret_cast<const char*>(&count), sizeof(_uint));

	for (const CScene::SCENENAVIGATIONINFO& info : _infoList)
	{
		WriteBinaryWString(out, info.resourceName);
		WriteSceneNavBakeOptions(out, info.bakeOptions);
		WriteSceneSerializedNavMesh(out, info.bakedNavMesh);
	}

	out.close();
	CDebug::Log(L"Save complete navdata: " + _filePath);
	return S_OK;
}

vector<CScene::SCENENAVIGATIONINFO> CResources::ReadSceneNavigationInfos(const wstring _binFileName)
{
	using namespace std;

	vector<CScene::SCENENAVIGATIONINFO> resultInfo = {};
	ifstream in(L"BinaryAssets/SceneData/" + _binFileName, ios::binary);
	if (!in.is_open())
		return {};

	_uint magic = 0;
	_uint version = 0;
	_uint count = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(_uint));
	in.read(reinterpret_cast<char*>(&version), sizeof(_uint));
	in.read(reinterpret_cast<char*>(&count), sizeof(_uint));

	if (!in || magic != 0x4E415631 || version < 1 || version > 2)
		return {};

	resultInfo.reserve(count);
	for (_uint i = 0; i < count; ++i)
	{
		CScene::SCENENAVIGATIONINFO info = {};
		info.resourceName = ReadBinaryWString(in);
		ReadSceneNavBakeOptions(in, info.bakeOptions);
		ReadSceneSerializedNavMesh(in, version, info.bakedNavMesh);
		if (!in)
			return {};

		if (!info.resourceName.empty())
			resultInfo.push_back(info);
	}

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

	const wstring sourceAssetPath = _infoList.empty() ? L"" : _infoList.front().sourceAssetPath;
	_uint sourceAssetPathSize = static_cast<_uint>(sourceAssetPath.size());
	out.write(reinterpret_cast<const char*>(&sourceAssetPathSize), sizeof(_uint));
	if (sourceAssetPathSize > 0)
		out.write(reinterpret_cast<const char*>(sourceAssetPath.data()), sizeof(wchar_t) * sourceAssetPathSize);

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

	_uint sourceAssetPathCount = 0;
	if (in.read(reinterpret_cast<char*>(&sourceAssetPathCount), sizeof(_uint)))
	{
		wstring sourceAssetPath = L"";
		if (sourceAssetPathCount > 0)
		{
			sourceAssetPath.resize(sourceAssetPathCount);
			in.read(reinterpret_cast<char*>(sourceAssetPath.data()), sizeof(wchar_t) * sourceAssetPathCount);
		}

		for (auto& info : infoList)
			info.sourceAssetPath = sourceAssetPath;
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

		_uint childLen = static_cast<_uint>(bone.childsId.size());
		out.write(reinterpret_cast<const char*>(&childLen), sizeof(_uint));
		if (childLen > 0)
			out.write(reinterpret_cast<const char*>(bone.childsId.data()), sizeof(_int) * childLen);

		_uint meshLen = static_cast<_uint>(bone.meshsId.size());
		out.write(reinterpret_cast<const char*>(&meshLen), sizeof(_uint));
		if (meshLen > 0)
			out.write(reinterpret_cast<const char*>(bone.meshsId.data()), sizeof(_int) * meshLen);
	}

	const wstring sourceAssetPath = _infoList.empty() ? L"" : _infoList.front().sourceAssetPath;
	_uint sourceAssetPathSize = static_cast<_uint>(sourceAssetPath.size());
	out.write(reinterpret_cast<const char*>(&sourceAssetPathSize), sizeof(_uint));
	if (sourceAssetPathSize > 0)
		out.write(reinterpret_cast<const char*>(sourceAssetPath.data()), sizeof(wchar_t) * sourceAssetPathSize);

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

	_uint sourceAssetPathCount = 0;
	if (in.read(reinterpret_cast<char*>(&sourceAssetPathCount), sizeof(_uint)))
	{
		wstring sourceAssetPath = L"";
		if (sourceAssetPathCount > 0)
		{
			sourceAssetPath.resize(sourceAssetPathCount);
			in.read(reinterpret_cast<char*>(sourceAssetPath.data()), sizeof(wchar_t) * sourceAssetPathCount);
		}

		for (auto& info : infoList)
			info.sourceAssetPath = sourceAssetPath;
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

	// Metadata: loop + speed per clip
	_uint metaMarker = 0x414E494D; // 'ANIM'
	out.write(reinterpret_cast<const char*>(&metaMarker), sizeof(_uint));
	for (const auto& clip : _infoList)
	{
		_bool loop = clip.loop;
		_float speed = clip.speed;
		out.write(reinterpret_cast<const char*>(&loop), sizeof(_bool));
		out.write(reinterpret_cast<const char*>(&speed), sizeof(_float));
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
	}

	// Read metadata (loop + speed) if present
	_uint metaMarker = 0;
	if (in.read(reinterpret_cast<char*>(&metaMarker), sizeof(_uint)) && metaMarker == 0x414E494D)
	{
		for (_uint c = 0; c < clipCount && c < clips.size(); ++c)
		{
			_bool loop = false;
			_float speed = 1.f;
			in.read(reinterpret_cast<char*>(&loop), sizeof(_bool));
			in.read(reinterpret_cast<char*>(&speed), sizeof(_float));
			clips[c].loop = loop;
			clips[c].speed = speed;
		}
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

vector<SkinnedMeshBundle> CResources::LoadSkinnedMeshBuffersOnPath(const wstring& _path, _int _filter)
{
	wstring normalizedPath = CEngineString::Replace(_path, L"\\", L"/");
	auto parts = CEngineString::Split(normalizedPath, L"/");

	if (parts.size() < 2)
	{
		CDebug::LogError(L"Failed LoadSkinnedMeshBuffersOnPath - invalid path: " + _path);
		return {};
	}

	const wstring folder = parts[parts.size() - 2];
	const wstring stem = fs::path(normalizedPath).stem().wstring();
	const wstring resourceName = stem + L" (MeshBuffer)";

	// Already loaded
	if (CScene* scene = CSceneManager::GetInstance().Get_CrtScene())
	{
		auto r = scene->Find_SkinnedMeshInfoResource(resourceName);
		if (!r.empty())
			return r;
	}

	const wstring skinnedDataPath = folder + L"_" + stem + L".skinneddata";

	if (!FileExists(L"BinaryAssets/SkinnedMeshData/" + skinnedDataPath))
	{
		if (FAILED(ConvertFBXToSkinnedBufferData(normalizedPath)))
		{
			CDebug::LogError(L"Failed LoadSkinnedMeshBuffersOnPath - convert failed: " + _path);
			return {};
		}
	}

	auto skinnedInfoList = ReadSkinnedBufferInfos(skinnedDataPath);
	return CreateSceneSkinnedBundle(resourceName, skinnedInfoList.initList, skinnedInfoList.skeletalList, _filter);
}

vector<CSkinnedMeshBuffer::SKINNEDSKELETAL> CResources::LoadSkinnedBonesOnPath(const wstring& _path)
{
	wstring normalizedPath = CEngineString::Replace(_path, L"\\", L"/");
	auto parts = CEngineString::Split(normalizedPath, L"/");

	if (parts.size() < 2)
	{
		CDebug::LogError(L"Failed LoadSkinnedBonesOnPath - invalid path: " + _path);
		return {};
	}

	const wstring folder = parts[parts.size() - 2];
	const wstring stem = fs::path(normalizedPath).stem().wstring();
	const wstring resourceName = stem + L" (MeshBuffer)";

	// Already loaded
	if (CScene* scene = CSceneManager::GetInstance().Get_CrtScene())
	{
		auto r = scene->Find_SkinnedBonesResource(resourceName);
		if (!r.empty())
			return r;
	}

	const wstring skinnedDataPath = folder + L"_" + stem + L".skinneddata";

	if (!FileExists(L"BinaryAssets/SkinnedMeshData/" + skinnedDataPath))
	{
		if (FAILED(ConvertFBXToSkinnedBufferData(normalizedPath)))
		{
			CDebug::LogError(L"Failed LoadSkinnedBonesOnPath - convert failed: " + _path);
			return {};
		}
	}

	auto skinnedInfoList = ReadSkinnedBufferInfos(skinnedDataPath);
	return skinnedInfoList.skeletalList;
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
	LoadResourceComplete_Game<CMeshBuffer>(L"Sphere (Mesh Buffer)", L"Sphere");
	LoadResourceComplete_Game<CMeshBuffer>(L"Cylinder (Mesh Buffer)", L"Cylinder");
	LoadResourceComplete_Game<CMeshBuffer>(L"Plane (Mesh Buffer)", L"Plane");
	LoadResourceComplete_Game<CMeshBuffer>(L"Quad (Mesh Buffer)", L"Quad");

	LoadResourceComplete_Game<CTexture>(L"DefaultSky (Texture)", L"../EngineResources/Image/DefaultSkyBox.png");

	CShader::SHADERDESC lineColorShaderDesc = { L"../EngineResources/Shader/DefaultLine.hlsl", L"",  VertexColorSkinnedBuffer::numElements, VertexColorSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"DefaultLine (Shader)", L"", &lineColorShaderDesc);

	CShader* dlShader = LoadOnGame<CShader>(L"DefaultLine (Shader)");
	CMaterial::MATERIALDESC dlMatDesc = { dlShader, false, false };
	LoadResourceComplete_Game<CMaterial>(L"DefaultLineMaterial (Material)", L"", &dlMatDesc);

	CMaterial::MATERIALDESC navOutlineMatDesc = { dlShader, false, false };
	CMaterial* navOutlineMat = LoadResourceComplete_Game<CMaterial>(L"NavigationOutlineMaterial (Material)", L"", &navOutlineMatDesc);
	if (navOutlineMat)
		navOutlineMat->Set_BaseColor(_float4(0.05f, 0.72f, 1.f, 1.f));

	CShader::SHADERDESC litShaderDesc = { L"../EngineResources/Shader/Lit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"Lit (Shader)", L"", &litShaderDesc);

	CShader* litShader = LoadOnGame<CShader>(L"Lit (Shader)");
	CMaterial::MATERIALDESC litMatDesc = { litShader, false, true };
	litMatDesc.customFloatValues.push_back({ L"gSmoothness", 0.f });
	litMatDesc.customVector2Values.push_back({ L"gTiling", { 1.f, 1.f } });
	litMatDesc.customVector2Values.push_back({ L"gOffset", { 0.f, 0.f } });
	LoadResourceComplete_Game<CMaterial>(L"Lit (Material)", L"", &litMatDesc);

	{
		CShader::SHADERDESC skyBoxShaderDesc = { L"../EngineResources/Shader/Skybox.hlsl", L"",  VertexTexNormalTangentBuffer::numElements, VertexTexNormalTangentBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"SkyBox (Shader)", L"", &skyBoxShaderDesc);

		CShader* skyBoxShader = LoadOnGame<CShader>(L"SkyBox (Shader)");
		CMaterial::MATERIALDESC skyMatDesc = { skyBoxShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"SkyBoxMaterial (Material)", L"", &skyMatDesc);

		CSkyBox::SKYBOXBUFFERDESC dskyDesk = { L"DefaultSky (Texture)" };
		LoadResourceComplete_Game<CSkyBox>(L"DefaultSky (SkyBox)", L"", &dskyDesk);
	}

	{
		CShader::SHADERDESC g_BufferlitShaderDesc = { L"../EngineResources/Shader/GBufferLit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"G_BufferLit (Shader)", L"", &g_BufferlitShaderDesc);

		CShader* g_BufferLitShader = LoadOnGame<CShader>(L"G_BufferLit (Shader)");
		CMaterial::MATERIALDESC g_BufferLitMatDesc = { g_BufferLitShader, false, true, true, true };
		g_BufferLitMatDesc.customFloatValues.push_back({ L"gOcculusion", 1.f });
		g_BufferLitMatDesc.customFloatValues.push_back({ L"gRoughness", 0.5f });
		g_BufferLitMatDesc.customFloatValues.push_back({ L"gMetallic", 0.f });
		g_BufferLitMatDesc.customIntValues.push_back({ L"gObjectID", 0 });
		g_BufferLitMatDesc.customVector2Values.push_back({ L"gTiling", {1.f, 1.f} });
		LoadResourceComplete_Game<CMaterial>(L"G_BufferLit (Material)", L"", &g_BufferLitMatDesc);
	}

	{
		CShader::SHADERDESC textureLODPreviewShaderDesc = { L"../EngineResources/Shader/TextureLODPreview.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"TextureLODPreview (Shader)", L"", &textureLODPreviewShaderDesc);

		CShader* textureLODPreviewShader = LoadOnGame<CShader>(L"TextureLODPreview (Shader)");
		CMaterial::MATERIALDESC textureLODPreviewMatDesc = { textureLODPreviewShader, false, false };
		textureLODPreviewMatDesc.customVector4Values.push_back({ L"gPreviewParams", {0.f, 1.f, 1.f, 0.f} });
		LoadResourceComplete_Game<CMaterial>(L"TextureLODPreview (Material)", L"", &textureLODPreviewMatDesc);
	}

	{
		CShader::SHADERDESC g_BufferCutoutLitShaderDesc = { L"../EngineResources/Shader/GbufferCutoutLit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"G_BufferCutoutLit (Shader)", L"", &g_BufferCutoutLitShaderDesc);

		CShader* g_BufferCutoutLitShader = LoadOnGame<CShader>(L"G_BufferCutoutLit (Shader)");
		CMaterial::MATERIALDESC g_BufferCutoutLitMatDesc = { g_BufferCutoutLitShader, true, true, true, true };
		g_BufferCutoutLitMatDesc.customFloatValues.push_back({ L"gOcculusion", 1.f });
		g_BufferCutoutLitMatDesc.customFloatValues.push_back({ L"gRoughness", 0.5f });
		g_BufferCutoutLitMatDesc.customFloatValues.push_back({ L"gMetallic", 0.f });
		g_BufferCutoutLitMatDesc.customIntValues.push_back({ L"gObjectID", 0 });
		g_BufferCutoutLitMatDesc.customVector2Values.push_back({ L"gTiling", {1.f, 1.f} });
		LoadResourceComplete_Game<CMaterial>(L"G_BufferCutoutLit (Material)", L"", &g_BufferCutoutLitMatDesc);
	}

	{
		CShader::SHADERDESC g_TransparentLitShaderDesc = { L"../EngineResources/Shader/G_TransparentLit.hlsl", L"", VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"G_TransparentLit (Shader)", L"", &g_TransparentLitShaderDesc);

		CShader* g_TransparentLitShader = LoadOnGame<CShader>(L"G_TransparentLit (Shader)");
		CMaterial::MATERIALDESC g_TransparentLitMatDesc = { g_TransparentLitShader, true, true, true, true, true };
		g_TransparentLitMatDesc.customFloatValues.push_back({ L"gOcculusion", 1.f });
		g_TransparentLitMatDesc.customFloatValues.push_back({ L"gRoughness", 0.5f });
		g_TransparentLitMatDesc.customFloatValues.push_back({ L"gMetallic", 0.f });
		g_TransparentLitMatDesc.customIntValues.push_back({ L"gObjectID", 0 });
		g_TransparentLitMatDesc.customVector2Values.push_back({ L"gTiling", {1.f, 1.f} });
		LoadResourceComplete_Game<CMaterial>(L"G_TransparentLit (Material)", L"", &g_TransparentLitMatDesc);
	}

	{
		CShader::SHADERDESC deferredPresentShaderDesc = { L"../EngineResources/Shader/DeferredPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		CShader* deferredPresentShader = LoadResourceComplete_Game<CShader>(L"DeferredPresent (Shader)", L"", &deferredPresentShaderDesc);

		CMaterial::MATERIALDESC deferredPresentMatDesc = { deferredPresentShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"DeferredPresent (Material)", L"", &deferredPresentMatDesc);
	}

	{
		CShader::SHADERDESC objectIDPresentShaderDesc = { L"../EngineResources/Shader/ObjectIDPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		CShader* objectIDPresentShader = LoadResourceComplete_Game<CShader>(L"ObjectIDPresent (Shader)", L"", &objectIDPresentShaderDesc);

		CMaterial::MATERIALDESC objectIDPresentMatDesc = { objectIDPresentShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"ObjectIDPresent (Material)", L"", &objectIDPresentMatDesc);
	}

	{
		CShader::SHADERDESC depthPresentShaderDesc = { L"../EngineResources/Shader/DepthPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		CShader* depthPresentShader = LoadResourceComplete_Game<CShader>(L"DepthPresent (Shader)", L"", &depthPresentShaderDesc);

		CMaterial::MATERIALDESC depthPresentMatDesc = { depthPresentShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"DepthPresent (Material)", L"", &depthPresentMatDesc);
	}

	{
		CShader::SHADERDESC shadowDepthPresentShaderDesc = { L"../EngineResources/Shader/ShadowDepthPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"ShadowDepthPresent (Shader)", L"", &shadowDepthPresentShaderDesc);

		CShader* shadowDepthPresentShader = LoadOnGame<CShader>(L"ShadowDepthPresent (Shader)");
		CMaterial::MATERIALDESC shadowDepthPresentMatDesc = { shadowDepthPresentShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"ShadowDepthPresent (Material)", L"", &shadowDepthPresentMatDesc);
	}

	{
		CShader::SHADERDESC shadowMaskPresentShaderDesc = { L"../EngineResources/Shader/ShadowMaskPresent.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"ShadowMaskPresent (Shader)", L"", &shadowMaskPresentShaderDesc);

		CShader* shadowMaskPresentShader = LoadOnGame<CShader>(L"ShadowMaskPresent (Shader)");
		CMaterial::MATERIALDESC shadowMaskPresentMatDesc = { shadowMaskPresentShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"ShadowMaskPresent (Material)", L"", &shadowMaskPresentMatDesc);
	}

	{
		CShader::SHADERDESC deferredCombineShaderDesc = { L"../EngineResources/Shader/DeferredCombine.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"DeferredCombine (Shader)", L"", &deferredCombineShaderDesc);

		CShader* deferredCombineShader = LoadOnGame<CShader>(L"DeferredCombine (Shader)");
		CMaterial::MATERIALDESC deferredCombineMatDesc = { deferredCombineShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"DeferredCombine (Material)", L"", &deferredCombineMatDesc);
	}

	{
		CShader::SHADERDESC deferredLightingCombinedShaderDesc = { L"../EngineResources/Shader/DeferredLightingCombined.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"DeferredLightingCombined (Shader)", L"", &deferredLightingCombinedShaderDesc);

		CShader* deferredLightingCombinedShader = LoadOnGame<CShader>(L"DeferredLightingCombined (Shader)");
		CMaterial::MATERIALDESC deferredLightingCombinedMatDesc = { deferredLightingCombinedShader, false, true };
		LoadResourceComplete_Game<CMaterial>(L"DeferredLightingCombined (Material)", L"", &deferredLightingCombinedMatDesc);
	}

	{
		CShader::SHADERDESC deferredDiffuseShaderDesc = { L"../EngineResources/Shader/DeferredDiffuse.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"DeferredDiffuse (Shader)", L"", &deferredDiffuseShaderDesc);

		CShader* deferredDiffuseShader = LoadOnGame<CShader>(L"DeferredDiffuse (Shader)");
		CMaterial::MATERIALDESC deferredDiffuseMatDesc = { deferredDiffuseShader, false, true };
		LoadResourceComplete_Game<CMaterial>(L"DeferredDiffuse (Material)", L"", &deferredDiffuseMatDesc);
	}

	{
		CShader::SHADERDESC deferredSpecularShaderDesc = { L"../EngineResources/Shader/DeferredSpecular.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"DeferredSpecular (Shader)", L"", &deferredSpecularShaderDesc);

		CShader* deferredSpecularShader = LoadOnGame<CShader>(L"DeferredSpecular (Shader)");
		CMaterial::MATERIALDESC deferredSpecularMatDesc = { deferredSpecularShader, false, true };
		LoadResourceComplete_Game<CMaterial>(L"DeferredSpecular (Material)", L"", &deferredSpecularMatDesc);
	}

	{
		CShader::SHADERDESC shadowDepthShaderDesc = { L"../EngineResources/Shader/ShadowDepth.hlsl", L"",  VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"ShadowDepth (Shader)", L"", &shadowDepthShaderDesc);

		CShader* shadowDepthShader = LoadOnGame<CShader>(L"ShadowDepth (Shader)");
		CMaterial::MATERIALDESC shadowDepthMatDesc = { shadowDepthShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"ShadowDepth (Material)", L"", &shadowDepthMatDesc);
	}

	{
		CShader::SHADERDESC shadowMaskShaderDesc = { L"../EngineResources/Shader/ShadowMask.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"ShadowMask (Shader)", L"", &shadowMaskShaderDesc);

		CShader* shadowMaskShader = LoadOnGame<CShader>(L"ShadowMask (Shader)");
		CMaterial::MATERIALDESC shadowMaskMatDesc = { shadowMaskShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"ShadowMask (Material)", L"", &shadowMaskMatDesc);
	}

	{
		CShader::SHADERDESC unlitColorShaderDesc = { L"../EngineResources/Shader/UnlitColor.hlsl", L"",  VertexSkinnedBuffer::numElements, VertexSkinnedBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"UnlitColor (Shader)", L"", &unlitColorShaderDesc);

		CShader* ulcShader = LoadOnGame<CShader>(L"UnlitColor (Shader)");
		CMaterial::MATERIALDESC ulcMatDesc = { ulcShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"UnlitMaterial (Material)", L"", &ulcMatDesc);
	}

	CShader* navOverlayShader = LoadOnGame<CShader>(L"UnlitColor (Shader)");
	CMaterial::MATERIALDESC navOverlayMatDesc = { navOverlayShader, false, false };
	CMaterial* navOverlayMat = LoadResourceComplete_Game<CMaterial>(L"NavigationOverlayMaterial (Material)", L"", &navOverlayMatDesc);
	if (navOverlayMat)
		navOverlayMat->Set_BaseColor(_float4(0.0f, 0.68f, 1.f, 0.30f));

	CShader::SHADERDESC outlineShaderDesc = { L"../EngineResources/Shader/Outline.hlsl", L"",  VertexSkinnedOutlineBuffer::numElements, VertexSkinnedOutlineBuffer::elementDesc };
	LoadResourceComplete_Game<CShader>(L"Outline (Shader)", L"", &outlineShaderDesc);
	
	{
		CShader::SHADERDESC dUIShaderDesc = { L"../EngineResources/Shader/DefaultUIInstanced.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"DefaultUI (Shader)", L"", &dUIShaderDesc);

		CShader* duiShader = LoadOnGame<CShader>(L"DefaultUI (Shader)");
		CMaterial::MATERIALDESC duiMatDesc = { duiShader, false, false };
		LoadResourceComplete_Game<CMaterial>(L"DefaultUIMaterial (Material)", L"", &duiMatDesc);
	}

	{
		CShader::SHADERDESC uiObjectIDShaderDesc = { L"../EngineResources/Shader/UIObjectID.hlsl", L"",  VertexTexColorBuffer::numElements, VertexTexColorBuffer::elementDesc };
		LoadResourceComplete_Game<CShader>(L"UIObjectID (Shader)", L"", &uiObjectIDShaderDesc);

		CShader* uiObjectIDShader = LoadOnGame<CShader>(L"UIObjectID (Shader)");
		CMaterial::MATERIALDESC uiObjectIDMatDesc = { uiObjectIDShader, false, false };
		uiObjectIDMatDesc.customIntValues.push_back({ L"gObjectID", 0 });
		LoadResourceComplete_Game<CMaterial>(L"UIObjectID (Material)", L"", &uiObjectIDMatDesc);
	}

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

	aiMatrix4x4 mat = _node->mTransformation;
	nodeInfo.transformation = _float4x4
	(
		mat.a1, mat.b1, mat.c1, mat.d1,
		mat.a2, mat.b2, mat.c2, mat.d2,
		mat.a3, mat.b3, mat.c3, mat.d3,
		mat.a4, mat.b4, mat.c4, mat.d4
	);

	nodeInfo.numMeshes = _node->mNumMeshes;
	for (_uint i = 0; i < _node->mNumMeshes; ++i)
		nodeInfo.meshsId.push_back(_node->mMeshes[i]);

	_outList.push_back(nodeInfo);
	_int currentId = nodeInfo.nodeId;

	for (_uint i = 0; i < _node->mNumChildren; ++i)
	{
		_int childId = static_cast<_int>(_outList.size());
		_outList[currentId].childsId.push_back(childId);
		_outList[currentId].numChild++;

		TraverseSkeleton(_node->mChildren[i], currentId, _outList);
	}
}
