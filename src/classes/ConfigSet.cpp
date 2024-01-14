#include "ConfigSet.h"

ConfigSet::ConfigSet(const std::basic_string<char> &file) {
	this->file = file;
	std::unique_lock l(mutex);
	std::ifstream i;
	i.open(file);

	std::string line;
	while (std::getline(i, line)) {
		list.insert(line);
	}
	i.close();
}

void ConfigSet::insert(const std::basic_string<char> &s) {
	std::unique_lock l(mutex);
	list.insert(s);
}

bool ConfigSet::contains(const std::basic_string<char> &s) {
	std::shared_lock l(mutex);
	return list.find(s) != list.end();
}

void ConfigSet::remove(const std::basic_string<char> &s) {
	std::unique_lock l(mutex);
	list.erase(s);
}

std::set<std::string> &ConfigSet::get_container() {
	return list;
}

std::shared_mutex &ConfigSet::get_mutex() {
	return mutex;
}

void ConfigSet::save() {
	std::unique_lock l(mutex);
	std::ofstream f;
	f.open(file);

	if (f.is_open()) {
		for (auto &s: list) {
			f << s << std::endl;
		}
		f.close();
	}
}