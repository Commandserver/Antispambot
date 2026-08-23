/************************************************************************************
 *
 * dpp-mysql - An asynchronous MySQL database wrapper for D++ bots
 *
 * Copyright 2020-2024 Craig Edwards <signing@dpp.dev>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#pragma once
#include <vector>
#include <map>
#include <string>
#include <variant>
#include <functional>
#include <dpp/dpp.h>

/**
 * @brief Database abstraction layer
 * Abstracts mysql C connector, supporting prepared statements and caching.
 */
namespace db {

	/**
	 * @brief Definition of a row in a result set
	 */
	using row = std::map<std::string, std::string>;

	/**
	 * @brief Definition of a result set. Supports iteration and accessing its
	 * rows via operator[] and at(). You can also insert new rows with emplace_back
	 * and push_back().
	 */
	struct resultset {

		/**
		 * Row values
		 */
		std::vector<row> rows;

		/**
		 * Error message of last query or an empty string on success
		 */
		std::string error;

		/**
		 * Number of affected rows, if an UPDATE, DELETE, INSERT
		 */
		size_t affected_rows{};

		/**
		 * Returns true if the query succeeded
		 * @return true if no error
		 */
		[[nodiscard]] inline bool ok() const {
			return error.empty();
		}

		/**
		 * Get a row by index
		 * @param index row to retrieve
		 * @return row
		 */
		[[nodiscard]] inline const row& operator[] (size_t index) const {
			return rows[index];
		}

		/**
		 * Get a row by index with range checking
		 * @param index row to retrieve
		 * @return row
		 */
		[[nodiscard]] inline const row& at(size_t index) const {
			return rows.at(index);
		}

		/**
		 * Emplace a new row
		 * @param r row
		 */
		inline void emplace_back(const row& r) {
			rows.emplace_back(r);
		}

		/**
		 * Push back a new row
		 * @param r row
		 */
		inline void push_back(const row& r) {
			rows.push_back(r);
		}

		/**
		 * Get the start iterator of the container,
		 * for iteration
		 * @return beginning of container
		 */
		[[nodiscard]] inline auto begin() const {
			return rows.begin();
		}

		/**
		 * Get the end iterator of the container,
		 * for iteration
		 * @return end of container
		 */
		[[nodiscard]] inline auto end() const {
			return rows.end();
		}

		/**
		 * True if the recordset is empty
		 * @return true if empty
		 */
		[[nodiscard]] inline bool empty() const {
			return rows.empty();
		}

		/**
		 * Number of rows in the recordset
		 * @return row count
		 */
		[[nodiscard]] inline size_t size() const {
			return rows.size();
		}
	};

	/**
	 * @brief A callback which happens when an asynchronous SQL query is completed
	 */
	using sql_query_callback = std::function<void(const resultset&)>;

	/**
	 * @brief Possible parameter types for SQL parameters
	 */
	using parameter_type = std::variant<float, std::string, uint64_t, int64_t, bool, int32_t, uint32_t, double>;

	/**
	 * @brief A list of database query parameters.
	 * These will be translated into prepared statement arguments.
	 */
	using paramlist = std::vector<parameter_type>;

	/**
	 * @brief Initialise database connection
	 *
	 * @param bot creating D++ cluster
	 */
	void init (dpp::cluster& bot);

	/**
	 * @brief Connect to database and set options
	 *
	 * @param host Database hostname
	 * @param user Database username
	 * @param pass Database password
	 * @param db Database schema name
	 * @param port Database port number
	 * @param socket unix socket path
	 * @return True if the database connection succeeded
	 *
	 * @note Unix socket and port number are mutually exclusive. If you set socket to a non-empty string,
	 * you should set port to 0 and host to `localhost`. This is a special value in the mysql client and
	 * causes a unix socket connection to occur. If you do not want to use unix sockets, keep the value
	 * as an empty string.
	 */
	bool connect(const std::string &host, const std::string &user, const std::string &pass, const std::string &db, int port = 3306, const std::string& socket = "");

	/**
	 * @brief Disconnect from database and free query cache
	 *
	 * @return true on successful disconnection
	 */
	bool close();

	/**
	 * @brief Run a mysql query, with automatic escaping of parameters to prevent SQL injection.
	 *
	 * @param format Format string, where each parameter should be indicated by a ? symbol
	 * @param parameters Parameters to prepare into the query in place of the ?'s
	 * @return result set
	 *
	 * The parameters given should be a vector of strings. You can instantiate this using "{}".
	 * The queries are cached as prepared statements and therefore do not need quote symbols
	 * to be placed around parameters in the query. These will be automatically added if required.
	 *
	 * For example:
	 *
	 * ```cpp
	 * 	db::query("UPDATE foo SET bar = ? WHERE id = ?", { "baz", 3 });
	 * ```
	 */
	resultset query(const std::string &format, const paramlist &parameters = {});

	/**
	 * @brief Run a mysql query asynchronously, with automatic escaping of parameters
	 * to prevent SQL injection. Call the callback on completion
	 *
	 * @param format Format string, where each parameter should be indicated by a ? symbol
	 * @param parameters Parameters to prepare into the query in place of the ?'s
	 * @param cb Callback to call on completion of the query. The callback will be passed
	 * the resultset as its parameter.
	 *
	 * The parameters given should be a vector of strings. You can instantiate this using "{}".
	 * The queries are cached as prepared statements and therefore do not need quote symbols
	 * to be placed around parameters in the query. These will be automatically added if required.
	 *
	 * @note If you can you should use co_query instead to avoid callback hell. co_query uses this
	 * internally, wrapping it with dpp::async<>.
	 */
	void query_callback(const std::string &format, const paramlist &parameters, const sql_query_callback& cb);

#ifdef DPP_CORO
	/**
	 * @brief Run a mysql query, with automatic escaping of parameters to prevent SQL injection.
	 * This is the coroutine asynchronous version of db::query. It will complete its dpp::async
	 * once the query has finished without blocking, by means of a separate thread.
	 *
	 * @param format Format string, where each parameter should be indicated by a ? symbol
	 * @param parameters Parameters to prepare into the query in place of the ?'s
	 * @return dpp::async which you can co_await to get the result set.
	 *
	 * @note It is the nature of asynchronous APIs like this that they cannot be atomic. Therefore
	 * do not combine transactions with asynchronous co_query. If you do, other queries may end up
	 * muddled into your transaction. This wrapper ensures the correct order of statements passed to
	 * co_query from the same origin thread, however statements from other threads, or statements via
	 * db::query() instead are not queued. Pick one interface, or the other!
	 *
	 * The parameters given should be a vector of strings. You can instantiate this using "{}".
	 * The queries are cached as prepared statements and therefore do not need quote symbols
	 * to be placed around parameters in the query. These will be automatically added if required.
	 *
	 * For example:
	 *
	 * ```cpp
	 * 	auto rs = co_await db::co_query("SELECT * FROM bigtable WHERE bar = ?", { "baz" });
	 * ```
	 */
	dpp::async<resultset> co_query(const std::string &format, const paramlist &parameters = {});
#endif

	/**
	 * @brief Run a mysql query, with automatic escaping of parameters to prevent SQL injection.
	 *
	 * @param format Format string, where each parameter should be indicated by a ? symbol
	 * @param parameters Parameters to prepare into the query in place of the ?'s
	 * @param lifetime How long to cache this query's resultset in memory for
	 * @return result set
	 *
	 * @note If the query is already cached in memory, the cached resultset will be returned instead
	 * of querying the database.
	 *
	 * The parameters given should be a vector of strings. You can instantiate this using "{}".
	 * The queries are cached as prepared statements and therefore do not need quote symbols
	 * to be placed around parameters in the query. These will be automatically added if required.
	 */
	resultset query(const std::string &format, const paramlist &parameters, double lifetime);

	/**
	 * @brief Returns number of affected rows from an UPDATE, INSERT, DELETE
	 *
	 * @note This value is by any db::query() call. Take a copy!
	 * @return size_t Number of affected rows
	 * @deprecated This is not coroutine-safe and you should use resultset::affected_rows instead
	 */
	[[deprecated("Use resultset::affected_rows instead")]] size_t affected_rows();

	/**
	 * @brief Returns the last error string.
	 *
	 * @note This value is by any db::query() call. Take a copy!
	 * @return const std::string& Error mesage
	 * @deprecated This is not coroutine-safe and you should use resultset::error instead
	 */
	[[deprecated("Use resultset::error instead")]] const std::string& error();

	/**
	 * @brief Returns the size of the query cache
	 *
	 * Prepared statement handles are stored in a std::map along with their metadata, so that
	 * they don't have to be re-prepared if they are executed repeatedly. This is a diagnostic
	 * and informational function which returns the size of that map.
	 *
	 * @return size_t Cache size
	 */
	size_t cache_size();

	/**
	 * @brief Returns total number of queries executed since connection was established
	 *
	 * @return size_t Query counter
	 */
	size_t query_count();

	/**
	 * @brief Start an SQL transaction.
	 * SQL transactions are atomic in nature, ALL other queries will be forced
	 * to wait. The transaction will be inserted into the queue to run as one atomic
	 * operation, meaning that db::co_query cannot disrupt it or insert queries in
	 * the middle of it, and db::query function calls that are not within the closure
	 * will be forced to wait.
	 *
	 * @param closure The transactional code to execute.
	 * @param callback Callback to call when the transaction completes
	 *
	 * @note The closure should only ever execute queries using db::query(),
	 * it should NOT use async queries/co_query() as these cannot be executed
	 * atomically.
	 * Returning false from the closure, or throwing any exception at all
	 * will roll back the transaction, else it will be committed when the
	 * closure ends.
	 * @warning The coroutine will execute asynchronously in a different thread.
	 * You should use the callback to be notified when it completes. The resultset
	 * passed as the parameter will be empty.
	 */
	void transaction(std::function<bool()> closure, sql_query_callback callback = {});

#ifdef DPP_CORO
	/**
	 * @brief Start an SQL transaction in a coroutine that can be awaited.
	 * SQL transactions are atomic in nature, ALL other queries will be forced
	 * to wait. The transaction will be inserted into the queue to run as one atomic
	 * operation, meaning that db::co_query cannot disrupt it or insert queries in
	 * the middle of it, and db::query function calls that are not within the closure
	 * will be forced to wait.
	 *
	 * @param closure The transactional code to execute.
	 * @return Awaitable, returns an empty resultset on completion of transaction
	 *
	 * @note The closure should only ever execute queries using db::query(),
	 * it should NOT use async queries/co_query() as these cannot be executed
	 * atomically.
	 * Returning false from the closure, or throwing any exception at all
	 * will roll back the transaction, else it will be committed when the
	 * closure ends.
	 */
	dpp::async<resultset> co_transaction(std::function<bool()> closure);
#endif
};