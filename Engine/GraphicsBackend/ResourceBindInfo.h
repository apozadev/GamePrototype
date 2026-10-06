#pragma once

#include <string>

#include "PipelineStage.h"

struct ResourceBindInfo
{
  PipelineStageFlags m_uStageFlags;
  std::string m_sName;
};
