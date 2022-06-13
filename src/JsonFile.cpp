#include "JsonFile.h"

JsonFile::JsonFile(const string &file) {
	this->file = file;
	unique_lock l(file_mutex);
	ifstream i;
	i.open(file);
	if (i.is_open()) {
		i >> content;
		i.close();
	}
}

shared_mutex &JsonFile::get_mutex() {
	return this->file_mutex;
}

void JsonFile::save() {
	unique_lock l(file_mutex);
	ofstream f;
	f.open(file);
	if (f.is_open()) {
		f << content;
		f.close();
	}
}
