#include <dpp/dpp.h>

using namespace std;

/**
 * Case Sensitive Implementation of endsWith()
 * It checks if the string 'mainStr' ends with given string 'toMatch'
 */
bool endsWith(const string &mainStr, const string &toMatch) {
	if (mainStr.size() >= toMatch.size() &&
		mainStr.compare(mainStr.size() - toMatch.size(), toMatch.size(), toMatch) == 0)
		return true;
	else
		return false;
}


/**
 * Make a readable time-string from seconds
 * @param seconds The amount of seconds to create a time-string for
 * @return The formatted time-string
 */
string stringifySeconds(uint32_t seconds) {
	uint32_t days = 0, hours = 0, minutes = 0;

	while (seconds >= 86400) {
		seconds -= 86400;
		days++;
	}
	while (seconds >= 3600) {
		seconds -= 3600;
		hours++;
	}
	while (seconds >= 60) {
		seconds -= 60;
		minutes++;
	}

	string result;
	if (days == 1) {
		result += to_string(days) + " day ";
	} else if (days > 1) {
		result += to_string(days) + " days ";
	}
	if (hours == 1) {
		result += to_string(hours) + " hour ";
	} else if (hours > 1) {
		result += to_string(hours) + " hours ";
	}
	if (minutes == 1) {
		result += to_string(minutes) + " minute ";
	} else if (minutes > 1) {
		result += to_string(minutes) + " minutes ";
	}
	if (seconds == 1) {
		result += to_string(seconds) + " second ";
	} else if (seconds > 1) {
		result += to_string(seconds) + " seconds ";
	}
	return result.erase(result.find_last_not_of(' ') + 1); // remove the ending space
}