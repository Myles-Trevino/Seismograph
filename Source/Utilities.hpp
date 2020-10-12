/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#pragma once

#include <string>
#include <vector>


namespace LV::Utilities
{
	// Strings.
	bool is_supported(const std::string& option,
		const std::vector<std::string>& supported_options);

	std::vector<std::string> split(const std::string& string, char delimiter = '\n');
}
