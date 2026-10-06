// Publisher: Fullike (https://github.com/fullike)
// Copyright 2024. All Rights Reserved.

#include "RoadBuilderTools.h"

DEFINE_LOG_CATEGORY(LogRoadBuilder);

void FRoadBuilderTools::StartupModule()
{
	// Nothing to register: this module only publishes the shared tool layer types.
}

void FRoadBuilderTools::ShutdownModule()
{
}

IMPLEMENT_MODULE(FRoadBuilderTools, RoadBuilderTools)
