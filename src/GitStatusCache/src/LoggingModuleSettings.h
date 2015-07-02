#pragma once

#include "LoggingSeverities.h"

namespace Logging
{
	struct LoggingModuleSettings
	{
		bool EnableConsoleLogging = true;
		bool EnableFileLogging = false;
		Severity MinimumSeverity = Severity::Info;
	};
}