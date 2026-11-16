#include "ObjLoader.h"

#include <charconv>
#include <cstring>
#include <string_view>

#include "GraphicsBackend/Vertex.h"
#include "Log.h"
#include "Memory/LinearAllocator.h"

namespace
{
  // 16-bit indices
  constexpr uint32_t MAX_VERTICES = 65536u;

  constexpr uint32_t EMPTY_SLOT = UINT32_MAX;

  struct ObjCounts
  {
    uint32_t uPositions;
    uint32_t uUVs;
    uint32_t uNormals;
    uint32_t uCorners;
    uint32_t uIndices;
  };

  struct ObjUV
  {
    float u, v;
  };

  // Indices into the attribute arrays, -1 if the corner doesn't have that attribute
  struct VertexKey
  {
    int32_t iPos;
    int32_t iUV;
    int32_t iNormal;
  };

  struct VertexSlot
  {
    VertexKey oKey;
    uint32_t uVertex;
  };

  struct LineReader
  {
    const char* pCurr;
    const char* pEnd;
    uint32_t uLine = 0u;

    bool Next(std::string_view& sLine_)
    {
      if (pCurr >= pEnd)
      {
        return false;
      }
      const char* pLineEnd = static_cast<const char*>(memchr(pCurr, '\n', pEnd - pCurr));
      if (!pLineEnd)
      {
        pLineEnd = pEnd;
      }
      sLine_ = std::string_view(pCurr, pLineEnd - pCurr);
      pCurr = pLineEnd + (pLineEnd < pEnd ? 1 : 0);
      uLine++;
      return true;
    }
  };
}

static bool IsSpace(char _c)
{
  return _c == ' ' || _c == '\t' || _c == '\r';
}

// Pops the next whitespace separated token from the front of sLine_. Empty at the end of the line.
static std::string_view NextToken(std::string_view& sLine_)
{
  size_t zStart = 0u;
  while (zStart < sLine_.size() && IsSpace(sLine_[zStart]))
  {
    zStart++;
  }
  size_t zEnd = zStart;
  while (zEnd < sLine_.size() && !IsSpace(sLine_[zEnd]))
  {
    zEnd++;
  }
  std::string_view sToken = sLine_.substr(zStart, zEnd - zStart);
  sLine_.remove_prefix(zEnd);
  return sToken;
}

static bool ParseFloat(std::string_view _sToken, float& f_)
{
  const char* p = _sToken.data();
  const char* pEnd = p + _sToken.size();
  if (p < pEnd && *p == '+')
  {
    p++;
  }
  auto [pParsed, eError] = std::from_chars(p, pEnd, f_);
  return eError == std::errc() && pParsed == pEnd && p != pEnd;
}

static bool ParseFloats(std::string_view& sLine_, float* pValues_, uint32_t _uCount)
{
  for (uint32_t i = 0u; i < _uCount; i++)
  {
    if (!ParseFloat(NextToken(sLine_), pValues_[i]))
    {
      return false;
    }
  }
  return true;
}

// OBJ indices are 1-based, or negative to count back from the last element defined so far
static bool ResolveIndex(int32_t _iRaw, uint32_t _uDefinedCount, int32_t& iIndex_)
{
  const int64_t iIndex = _iRaw > 0 ? static_cast<int64_t>(_iRaw) - 1 : static_cast<int64_t>(_uDefinedCount) + _iRaw;
  if (_iRaw == 0 || iIndex < 0 || iIndex >= static_cast<int64_t>(_uDefinedCount))
  {
    return false;
  }
  iIndex_ = static_cast<int32_t>(iIndex);
  return true;
}

// v, v/vt, v//vn or v/vt/vn
static bool ParseCorner(std::string_view _sToken, const ObjCounts& _oDefined, VertexKey& oKey_)
{
  const char* p = _sToken.data();
  const char* pEnd = p + _sToken.size();

  int32_t aRaw[3] = { 0, 0, 0 };
  for (int i = 0; i < 3; i++)
  {
    if (p < pEnd && *p != '/')
    {
      auto [pParsed, eError] = std::from_chars(p, pEnd, aRaw[i]);
      if (eError != std::errc())
      {
        return false;
      }
      p = pParsed;
    }
    if (i == 2 || p == pEnd || *p != '/')
    {
      break;
    }
    p++;
  }

  if (p != pEnd || !ResolveIndex(aRaw[0], _oDefined.uPositions, oKey_.iPos))
  {
    return false;
  }

  oKey_.iUV = -1;
  oKey_.iNormal = -1;
  return (aRaw[1] == 0 || ResolveIndex(aRaw[1], _oDefined.uUVs, oKey_.iUV))
    && (aRaw[2] == 0 || ResolveIndex(aRaw[2], _oDefined.uNormals, oKey_.iNormal));
}

static bool CountElements(const char* _pText, size_t _zSize, ObjCounts& oCounts_)
{
  oCounts_ = {};

  LineReader oReader = { _pText, _pText + _zSize };
  std::string_view sLine;
  while (oReader.Next(sLine))
  {
    const std::string_view sKeyword = NextToken(sLine);
    if (sKeyword == "v")
    {
      oCounts_.uPositions++;
    }
    else if (sKeyword == "vt")
    {
      oCounts_.uUVs++;
    }
    else if (sKeyword == "vn")
    {
      oCounts_.uNormals++;
    }
    else if (sKeyword == "f")
    {
      uint32_t uFaceCorners = 0u;
      while (!NextToken(sLine).empty())
      {
        uFaceCorners++;
      }
      if (uFaceCorners < 3u)
      {
        LogError("OBJ line %u: face with %u corners", oReader.uLine, uFaceCorners);
        return false;
      }
      oCounts_.uCorners += uFaceCorners;
      oCounts_.uIndices += (uFaceCorners - 2u) * 3u;
    }
  }

  if (oCounts_.uIndices == 0u)
  {
    LogError("OBJ has no faces");
    return false;
  }
  return true;
}

template<typename T>
static bool AllocateArray(LinearAllocator* _pAllocator, size_t _zCount, T*& pArray_)
{
  pArray_ = static_cast<T*>(_pAllocator->Allocate(sizeof(T) * _zCount, alignof(T)));
  if (!pArray_ && _zCount > 0u)
  {
    LogError("OBJ: not enough memory for %zu elements of %zu bytes", _zCount, sizeof(T));
    return false;
  }
  return true;
}

static uint32_t HashKey(const VertexKey& _oKey)
{
  return (static_cast<uint32_t>(_oKey.iPos) * 73856093u)
    ^ (static_cast<uint32_t>(_oKey.iUV) * 19349663u)
    ^ (static_cast<uint32_t>(_oKey.iNormal) * 83492791u);
}

// Fills the vertices and indices of oMesh_ (already allocated for the worst case).
// Temporary arrays live in _pAllocator only during the call.
static bool BuildMesh(const char* _pText, size_t _zSize, const ObjCounts& _oCounts, LinearAllocator* _pAllocator, ObjMesh& oMesh_)
{
  ScopedRollback oTemp(_pAllocator);

  VtxVec3* pPositions = nullptr;
  ObjUV* pUVs = nullptr;
  VtxVec3* pNormals = nullptr;
  if (!AllocateArray(_pAllocator, _oCounts.uPositions, pPositions)
    || !AllocateArray(_pAllocator, _oCounts.uUVs, pUVs)
    || !AllocateArray(_pAllocator, _oCounts.uNormals, pNormals))
  {
    return false;
  }

  // Power of two, at most half full
  const uint32_t uMaxVertices = _oCounts.uCorners < MAX_VERTICES ? _oCounts.uCorners : MAX_VERTICES;
  uint32_t uSlotCount = 1u;
  while (uSlotCount < uMaxVertices * 2u)
  {
    uSlotCount <<= 1u;
  }
  VertexSlot* pSlots = nullptr;
  if (!AllocateArray(_pAllocator, uSlotCount, pSlots))
  {
    return false;
  }
  for (uint32_t i = 0u; i < uSlotCount; i++)
  {
    pSlots[i].uVertex = EMPTY_SLOT;
  }

  ObjCounts oDefined = {};
  oMesh_.uVertexCount = 0u;
  oMesh_.uIndexCount = 0u;

  LineReader oReader = { _pText, _pText + _zSize };
  std::string_view sLine;
  while (oReader.Next(sLine))
  {
    const std::string_view sKeyword = NextToken(sLine);
    if (sKeyword == "v")
    {
      float aValues[3];
      if (!ParseFloats(sLine, aValues, 3u))
      {
        LogError("OBJ line %u: invalid position", oReader.uLine);
        return false;
      }
      pPositions[oDefined.uPositions++] = { aValues[0], aValues[1], aValues[2] };
    }
    else if (sKeyword == "vt")
    {
      ObjUV oUV = { 0.f, 0.f };
      const std::string_view sU = NextToken(sLine);
      const std::string_view sV = NextToken(sLine);
      if (!ParseFloat(sU, oUV.u) || (!sV.empty() && !ParseFloat(sV, oUV.v)))
      {
        LogError("OBJ line %u: invalid texture coordinate", oReader.uLine);
        return false;
      }
      pUVs[oDefined.uUVs++] = { oUV.u, 1.f - oUV.v };
    }
    else if (sKeyword == "vn")
    {
      float aValues[3];
      if (!ParseFloats(sLine, aValues, 3u))
      {
        LogError("OBJ line %u: invalid normal", oReader.uLine);
        return false;
      }
      pNormals[oDefined.uNormals++] = { aValues[0], aValues[1], aValues[2] };
    }
    else if (sKeyword == "f")
    {
      uint32_t uFirst = 0u;
      uint32_t uPrev = 0u;
      uint32_t uCorner = 0u;
      for (std::string_view sToken = NextToken(sLine); !sToken.empty(); sToken = NextToken(sLine), uCorner++)
      {
        VertexKey oKey;
        if (!ParseCorner(sToken, oDefined, oKey))
        {
          LogError("OBJ line %u: invalid face corner '%.*s'", oReader.uLine, static_cast<int>(sToken.size()), sToken.data());
          return false;
        }

        uint32_t uSlot = HashKey(oKey) & (uSlotCount - 1u);
        while (pSlots[uSlot].uVertex != EMPTY_SLOT && memcmp(&pSlots[uSlot].oKey, &oKey, sizeof(VertexKey)) != 0)
        {
          uSlot = (uSlot + 1u) & (uSlotCount - 1u);
        }

        if (pSlots[uSlot].uVertex == EMPTY_SLOT)
        {
          if (oMesh_.uVertexCount == MAX_VERTICES)
          {
            LogError("OBJ has more than %u unique vertices (16-bit indices)", MAX_VERTICES);
            return false;
          }

          Vertex& rVertex = oMesh_.pVertices[oMesh_.uVertexCount];
          rVertex = {};
          rVertex.m_vPos = pPositions[oKey.iPos];
          rVertex.m_vColor = { 1.f, 1.f, 1.f };
          if (oKey.iUV >= 0)
          {
            rVertex.m_vUv = { pUVs[oKey.iUV].u, pUVs[oKey.iUV].v, 0.f };
          }
          if (oKey.iNormal >= 0)
          {
            rVertex.m_vNormal = pNormals[oKey.iNormal];
          }

          pSlots[uSlot].oKey = oKey;
          pSlots[uSlot].uVertex = oMesh_.uVertexCount++;
        }

        const uint32_t uVertex = pSlots[uSlot].uVertex;
        if (uCorner == 0u)
        {
          uFirst = uVertex;
        }
        else if (uCorner >= 2u)
        {
          oMesh_.pIndices[oMesh_.uIndexCount++] = static_cast<uint16_t>(uFirst);
          oMesh_.pIndices[oMesh_.uIndexCount++] = static_cast<uint16_t>(uPrev);
          oMesh_.pIndices[oMesh_.uIndexCount++] = static_cast<uint16_t>(uVertex);
        }
        uPrev = uVertex;
      }
    }
  }

  return true;
}

bool ParseObj(const char* _pText, size_t _zSize, LinearAllocator* _pAllocator, ObjMesh& oMesh_)
{
  oMesh_ = {};

  ObjCounts oCounts;
  if (!CountElements(_pText, _zSize, oCounts))
  {
    return false;
  }

  ScopedRollback oRollback(_pAllocator);

  const uint32_t uMaxVertices = oCounts.uCorners < MAX_VERTICES ? oCounts.uCorners : MAX_VERTICES;
  ObjMesh oMesh = {};
  if (!AllocateArray(_pAllocator, uMaxVertices, oMesh.pVertices)
    || !AllocateArray(_pAllocator, oCounts.uIndices, oMesh.pIndices)
    || !BuildMesh(_pText, _zSize, oCounts, _pAllocator, oMesh))
  {
    return false;
  }

  oRollback.Commit();
  oMesh_ = oMesh;
  return true;
}
