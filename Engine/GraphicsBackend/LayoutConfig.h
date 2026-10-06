#pragma once

#include <inttypes.h>

enum class LayoutConfigElementType
{
  UBUFFER,
  TEXTURE
};

struct LayoutConfigElement
{
  const char* sName;
  uint32_t uBinding;
  LayoutConfigElementType eType;
  PipelineStageFlags uStageFlags;
};
