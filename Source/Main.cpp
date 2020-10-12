/*
	Copyright Myles Trevino
	Licensed under the Apache License, Version 2.0
	https://www.apache.org/licenses/LICENSE-2.0
*/


#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <regex>
#include <fstream>

#include <curl/curl.h>

#include "Constants.hpp"
#include "Utilities.hpp"
#include "Request.hpp"


struct Station
{
	std::string network;
	std::string station;
	std::string latitude;
	std::string longitude;
	bool invalid;
};

struct Channel
{
	std::string channel;
	std::string location;
};


const std::vector<std::string> valid_channel_types{"BHZ", "HNZ"};

float latitude;
float longitude;
std::string start_date;
std::string start_time;
int duration;

float radius;
bool first_attempt;
std::map<std::string, Station> stations;
Station selected_station;
Channel selected_channel;

// Parses the given response, first separating by line and then tokenizing.
std::vector<std::vector<std::string>> parse(
	const std::string& response, char separator, int count = 8)
{
	// Separate by line.
	std::vector<std::string> lines{LV::Utilities::split(response, '\n')};
	lines.erase(lines.begin());
	lines.pop_back();

	// Parse the lines.
	std::vector<std::vector<std::string>> result;
	for(const std::string& station_string : lines)
	{
		result.emplace_back(LV::Utilities::split(station_string, separator));

		if(result.back().size() != count)
			throw std::runtime_error{"Failed to parse the data."};
	}

	// Return.
	return result;
}


// Finds the available stations.
bool find_available_stations()
{
	// Send the request.
	std::cout<<"\nFinding stations that were operational on "<<start_date<<
		" near "<<latitude<<", "<<longitude<<" within a radius of "<<radius<<"...\n";

	std::stringstream station_query;
	station_query<<"http://service.iris.edu/fdsnws/station/1/query?latitude="<<latitude
		<<"&longitude="<<longitude<<"&maxradius="<<radius<<"&starttime="<<start_date
		<<"&endtime="<<start_date<<"&nodata=404&format=text";

	// If no stations were found, return false.
	std::string response{LV::Request::request(station_query.str())};
	if(response.find("Error 404") != std::string::npos) return false;

	// Otherwise, parse the station data.
	std::vector<std::vector<std::string>> raw_stations{parse(response, '|')};

	int new_stations{0};
	for(const std::vector<std::string>& raw_station : raw_stations)
	{
		Station station{raw_station[0], raw_station[1],
			raw_station[2], raw_station[3], false};

		std::string key{station.network+station.station};

		if(stations.find(key) == stations.end())
		{
			++new_stations;
			stations.emplace(key, station);
		}
	}

	std::cout<<"Found "<<stations.size()<<" new stations.\n";
	return true;
}


// Finds the first active, usable channel.
bool find_usable_channel()
{
	std::cout<<"Searching the stations for usable "
		"channels that were active on "<<start_date<<"...\n";

	// For each station...
	for(auto& iterator : stations)
	{
		Station* station{&iterator.second};

		// Skip invalid stations.
		if(station->invalid) continue;

		// Query for all channels that were active at the given time.
		std::cout<<"Checking "<<station->network<<" "<<station->station<<" ("
			<<station->latitude<<", "<<station->longitude<<")'s channels... ";

		std::stringstream channel_query;
		channel_query<<"https://service.iris.edu/fdsnws/availability/1/query?network="
			<<station->network<<"&station="<<station->station<<"&starttime="
			<<start_date<<"&endtime="<<start_date<<"&nodata=404";

		std::string response{LV::Request::request(channel_query.str())};

		// If no channels are available, invalidate the station and continue.
		if(response.find("Error 404") != std::string::npos)
		{
			std::cout<<"No channels were active.\n";
			station->invalid = true;
			continue;
		}

		// Otherwise, parse and iterate through the channels.
		std::vector<std::vector<std::string>> raw_channels{parse(response, ' ')};

		for(const std::vector<std::string>& raw_channel : raw_channels)
		{
			Channel channel{raw_channel[3], raw_channel[2]};

			if(LV::Utilities::is_supported(channel.channel, valid_channel_types))
			{
				selected_station = *station;
				selected_channel = channel;
				std::cout<<"Found an active "<<selected_channel.channel<<" channel.\n";
				return true;
			}
		}

		// Otherwise, invalidate the station.
		station->invalid = true;
		std::cout<<"No usable channel types.\n";
	}

	return false;
}


// Saves a WAV of the data that was found.
void save_wav()
{
	// Send the request.
	std::cout<<"Retrieving "<<duration<<" seconds of data starting at "<<start_time<<" on "
		<<start_date<<" from "<<selected_station.network<<selected_station.station<<" ("
		<<selected_station.latitude<<", "<<selected_station.longitude<<")'s "
		<<selected_channel.channel<<" channel.\n";

	std::stringstream wav_query;
	wav_query<<"http://service.iris.edu/irisws/timeseries/1/query?output=audio"
		"&net="<<selected_station.network<<"&sta="<<selected_station.station
		<<"&loc="<<selected_channel.location<<"&cha="<<selected_channel.channel
		<<"&starttime="<<start_date<<"T"<<start_time<<"&duration="<<duration;

	// Check that the response is a RIFF WAV.
	std::string response{LV::Request::request(wav_query.str())};
	if(response.substr(0, 4) != "RIFF")
		throw std::runtime_error{"Failed to download the data."};

	// Save the file.
	std::stringstream file_name;
	std::string converted_time{start_time};
	std::replace(converted_time.begin(), converted_time.end(), ':', '-');
	file_name<<selected_station.network<<" "<<selected_station.station<<" "
		<<selected_channel.channel<<" "<<start_date<<" "<<converted_time<<" "
		<<duration<<".wav";

	std::ofstream file{file_name.str(), std::ios::out|std::ios::binary};
	if(!file) throw std::runtime_error{"Failed to save the file."};
	file<<response;
	file.close();

	std::cout<<"Saved as \""<<file_name.str()<<"\".\n";
}


// Main.
int main()
{
	// Initialize.
	curl_global_init(CURL_GLOBAL_ALL);

	try
	{
		// Print the startup message.
		std::cout<<LV::Constants::program_name<<" "<<LV::Constants::program_version
		<<"\n\nCopyright Myles Trevino"
		<<"\nLicensed under the Apache License, Version 2.0"
		<<"\nhttps://www.apache.org/licenses/LICENSE-2.0\n";

		// Prompt for input.
		std::cout<<"\n> ";
		std::string input;
		std::getline(std::cin, input);

		// Parse and validate the input.
		std::vector<std::string> tokens{LV::Utilities::split(input, ' ')};

		if(tokens.size() != 5) throw std::runtime_error{"Usage: <Latitude> <Longitude> "
			"<Date> <Time> <Duration>. \"Date\" must be in YYYY-MM-DD format. \"Time\" must "
			"be in HH:MM:SS format (24-hour). \"Duration\" is in seconds. "
			"Example: \"41.967 -71.188 2017-03-01 12:00:00 1800\"."};

		latitude = std::stof(tokens[0]);
		longitude = std::stof(tokens[1]);

		start_date = tokens[2];
		if(!std::regex_match(start_date, std::regex("^\\d{4}-\\d{2}-\\d{2}$")))
			throw std::runtime_error{"Invalid date format."};

		start_time = tokens[3];
		if(!std::regex_match(start_time, std::regex("^\\d{2}:\\d{2}:\\d{2}$")))
			throw std::runtime_error{"Invalid time format."};

		duration = std::stoi(tokens[4]);

		// Search...
		radius = .1f;
		first_attempt = true;

		while(true)
		{
			// Increase the radius each subsequent attempt.
			if(!first_attempt)
			{
				std::cout<<"\nRetrying. Increasing the search "
					"radius from "<<radius<<" to "<<radius*2<<".";

				radius *= 2;
			}

			// Find the active stations.
			if(!find_available_stations())
			{
				std::cout<<"No stations were operational.\n";
				first_attempt = false;
				continue;
			}

			// Find the first active, usable channel.
			if(!find_usable_channel())
			{
				std::cout<<"None of the stations had usable channels.\n";
				first_attempt = false;
				continue;
			}

			// Save a WAV of the data that was found.
			save_wav();
			break;
		}
	}
	catch(std::exception& error){ std::cout<<"\nError: "<<error.what()<<'\n'; }
	catch(...){ std::cout<<"\nError: Unhandled exception.\n"; }

	std::cout<<'\n';
	system("pause");

	// Destroy.
	curl_global_cleanup();
}