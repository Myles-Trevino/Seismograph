/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include "Utilities.hpp"

#include <sstream>

#include "Constants.hpp"


bool LV::Utilities::is_supported(const std::string& option,
	const std::vector<std::string>& supported_options)
{
	for(const std::string& supported_option : supported_options)
		if(option == supported_option) return true;

	return false;
}


std::vector<std::string> LV::Utilities::split(
	const std::string& string, char delimiter)
{
	std::istringstream stream{string};

	std::vector<std::string> results;
	while(stream.good())
	{
		std::string line;
		std::getline(stream, line, delimiter);
		results.emplace_back(line);
	}

	return results;
}
