#pragma once

#include "PipelineStage.h"

struct ResourceBindInfo
{
  PipelineStageFlags m_uStageFlags;
  const char* m_sName; // Only read during the call it is passed to
};
