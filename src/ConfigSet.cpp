#include "ConfigSet.h"

ConfigSet::ConfigSet(const basic_string<char> &file) {
	this->file = file;
	unique_lock l(mutex);
	ifstream i;
	i.open(file);

	std::string line;
	while (std::getline(i, line)) {
		list.insert(line);
	}
	i.close();
}

void ConfigSet::insert(const basic_string<char> &s) {
	unique_lock l(mutex);
	list.insert(s);
}

bool ConfigSet::contains(const basic_string<char> &s) {
	shared_lock l(mutex);
	return list.find(s) != list.end();
}

void ConfigSet::remove(const basic_string<char> &s) {
	unique_lock l(mutex);
	list.erase(s);
}

set<string> &ConfigSet::get_container() {
	return list;
}

shared_mutex &ConfigSet::get_mutex() {
	return mutex;
}

void ConfigSet::save() {
	unique_lock l(mutex);
	ofstream f;
	f.open(file);

	if (f.is_open()) {
		for (auto &s: list) {
			f << s << endl;
		}
		f.close();
	}
}