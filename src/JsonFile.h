#pragma once
#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

/**
 * editable json config handler
 */
class JsonFile {
private:
	std::string file;
	std::shared_mutex file_mutex;

public:
	json content;

	/**
	 * @param file The json-file.
	 */
	JsonFile(const std::string &file);

	/**
	 * @brief Return the cache's locking mutex.
	 *
	 * @note If you are only reading from the cache's container, wrap this
	 * mutex in `std::shared_lock`, else wrap it in a `std::unique_lock`.
	 * Shared locks will allow for multiple readers whilst blocking writers,
	 * and unique locks will allow only one writer whilst blocking readers
	 * and writers.
	 *
	 * @return The mutex used to protect the container
	 */
	std::shared_mutex &get_mutex();

	/**
	 * Write the content into the file
	 */
	void save();
};
