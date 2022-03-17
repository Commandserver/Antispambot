#include <dpp/dpp.h>
#include <set>

using namespace std;

/**
 * Configuration list of unique strings. Saved in a raw txt file.
 * Each line in the file is parsed to an item in this ConfigSet.
 */
class ConfigSet {
private:
	string file;
	set<string> list;
	shared_mutex mutex;
public:
	/**
	 * @param file The filename
	 */
	ConfigSet(const basic_string<char> &file);

	/**
	 * Inserts a string to the set unless it already exists
	 * @param s The string to insert
	 */
	void insert(const basic_string<char> &s);

	/**
	 * Check if the set contains a value
	 * @param s The value to check
	 * @return
	 */
	bool contains(const basic_string<char> &s);

	/**
	 * Remove a string from the set
	 * @param s The string to remove
	 */
	void remove(const basic_string<char> &s);

	/**
	 * Use get_mutex to protect the container with a shared_mutex
	 * @return The reference to the internal string set
	 */
	set<string> &get_container();

	/**
	 * @return The mutex of the set
	 */
	shared_mutex &get_mutex();

	/**
	 * Write the set to the file
	 */
	void save();
};
