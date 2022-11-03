#include "JsonFile.h"

JsonFile::JsonFile(const std::string &file) {
	this->file = file;
	std::unique_lock l(file_mutex);
	std::ifstream i;
	i.open(file);
	if (i.is_open()) {
		i >> content;
		i.close();
	}
}

std::shared_mutex &JsonFile::get_mutex() {
	return this->file_mutex;
}

void JsonFile::save() {
	std::unique_lock l(file_mutex);
	std::ofstream f;
	f.open(file);
	if (f.is_open()) {
		f << content;
		f.close();
	}
}
