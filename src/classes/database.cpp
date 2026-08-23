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

#include "database.h"
#include "config.h"
#include <mysql/mysql.h>
#include <spdlog/spdlog.h>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <atomic>

#ifdef MARIADB_VERSION_ID
	#define CONNECT_STRING "SET @@SESSION.max_statement_time=3000"
#else
	#define CONNECT_STRING "SET @@SESSION.max_execution_time=3000"
#endif

namespace db {

	using namespace std::literals::chrono_literals;

	/**
	 * @brief Represents a cached prepared statement.
	 * We need to store the MYSQL_STMT*, the bound variable pointers,
	 * and the lengths, plus buffers for char* bound variables.
	 */
	struct cached_query {
		/**
		 * @brief If true, this query expects results, e.g. SELECT, EXPLAIN
		 */
		bool expects_results{false};

		/**
		 * @brief The mysql statement handle
		 */
		MYSQL_STMT* st{nullptr};

		/**
		 * @brief C pointers to bound variables
		 */
		MYSQL_BIND* bindings{nullptr};

		/**
		 * @brief C pointers to bound variable lengths
		 */
		unsigned long* lengths{nullptr};

		/**
		 * @brief Used to represent a set of C string buffers
		 */
		std::vector<std::string> bufs;
	};

	/**
	 * @brief MySQL database connection
	 */
	MYSQL connection;

	/**
	 * @brief Database mutex
	 * one connection may only be accessed by one thread at a time!
	 */
	std::mutex db_mutex;

	/**
	 * @brief Last error string from MySQL
	 */
	std::string last_error;

	/**
	 * @brief Total number of queries since connection
	 */
	size_t query_total{0};

	/**
	 * @brief Number of affected rows from last INSERT, UPDATE or DELETE
	 */
	size_t rows_affected{0};

	/**
	 * @brief Creating D++ cluster, used for logging
	 */
	dpp::cluster* creator{nullptr};

	std::atomic<bool> transaction_in_progress = false;
	std::function<void()> transaction_function = {};
	thread_local bool holds_transaction_lock = false;

	/**
	 * @brief Query cache, a map of cached_query
	 */
	std::map<std::string, cached_query> cached_queries;

	std::condition_variable sql_worker_cv;

	/**
	 * @brief Cached query result parameters
	 */
	struct cached_query_results {
		std::string format;
		paramlist parameters;
		sql_query_callback callback;
	};

	/**
	 * @brief Cached query result records with expiry
	 */
	struct cached_query_result_set {
		const resultset results;
		const double expiry;
	};

	std::queue<cached_query_results> sql_query_queue;
	std::mutex query_queue_mtx;

	template<class> inline constexpr bool always_false_v = false;

	struct cached_query_hash {
		std::size_t operator()(const cached_query_results& k) const {
			size_t x = std::hash<std::string>()(k.format);
			for (const auto& param : k.parameters) {
				std::visit([&x](auto &&p) {
					using T = std::decay_t<decltype(p)>;
					// float, std::string, uint64_t, int64_t, bool, int32_t, uint32_t, double
					if constexpr (std::is_same_v<T, float>) {
						x ^= std::hash<float>()(p);
					} else if constexpr (std::is_same_v<T, std::string>) {
						x ^= std::hash<std::string>()(p);
					} else if constexpr (std::is_same_v<T, uint64_t>) {
						x ^= std::hash<uint64_t>()(p);
					} else if constexpr (std::is_same_v<T, int64_t>) {
						x ^= std::hash<int64_t>()(p);
					} else if constexpr (std::is_same_v<T, bool>) {
						x ^= std::hash<bool>()(p);
					} else if constexpr (std::is_same_v<T, int32_t>) {
						x ^= std::hash<int32_t>()(p);
					} else if constexpr (std::is_same_v<T, uint32_t>) {
						x ^= std::hash<uint32_t>()(p);
					} else if constexpr (std::is_same_v<T, double>) {
						x ^= std::hash<double>()(p);
					} else {
						static_assert(always_false_v<T>, "non-exhaustive visitor!");
					}
				}, param);
			}
			return x;
		}
	};

	struct cached_query_equal {
		bool operator()(const cached_query_results& lhs, const cached_query_results& rhs) const {
			if (lhs.format != rhs.format || lhs.parameters.size() != rhs.parameters.size()) {
				return false;
			}
			return lhs.parameters == rhs.parameters;
		}
	};

	/**
	 * @brief A map of cached, executed queries with results and expiries
	 */
	std::unordered_map<cached_query_results, cached_query_result_set, cached_query_hash, cached_query_equal> cached_query_res;

	size_t cache_size() {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return cached_queries.size();
	}

	size_t query_count() {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return query_total;
	}

	/**
	 * @brief This is an internal connect function which has no locking, there is no public interface for this
	 *
	 * @param host hostname
	 * @param user username
	 * @param pass password
	 * @param db database
	 * @param port port
	 * @param socket unix socket
	 */
	bool unsafe_connect(const std::string &host, const std::string &user, const std::string &pass, const std::string &db, int port, const std::string &socket) {
		if (mysql_init(&connection) != nullptr) {
			mysql_options(&connection, MYSQL_INIT_COMMAND, CONNECT_STRING);
			int opts = CLIENT_MULTI_RESULTS | CLIENT_MULTI_STATEMENTS | CLIENT_REMEMBER_OPTIONS | CLIENT_IGNORE_SIGPIPE;
			bool result = mysql_real_connect(&connection, host.c_str(), user.c_str(), pass.c_str(), db.c_str(), port, socket.empty() ? nullptr : socket.c_str(), opts);
			signal(SIGPIPE, SIG_IGN);
			return result;
		} else {
			last_error = "mysql_init() failed";
			return false;
		}
	}

	bool connect(const std::string &host, const std::string &user, const std::string &pass, const std::string &db, int port, const std::string &socket) {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return unsafe_connect(host, user, pass, db, port, socket);
	}

	void query_callback(const std::string &format, const paramlist &parameters, const sql_query_callback& cb) {
		{
			std::unique_lock<std::mutex> queue_lock(query_queue_mtx);
			sql_query_queue.emplace(std::move(cached_query_results{.format = format, .parameters = parameters, .callback = cb}));
		}
		sql_worker_cv.notify_one();
	}

#ifdef DPP_CORO
	dpp::async<resultset> co_query(const std::string &format, const paramlist &parameters) {
		auto r = dpp::async<resultset>{ [format, parameters] <typename C> (C &&cc) { return query_callback(format, parameters, std::forward<C>(cc)); }};
	}
#endif

	void init (dpp::cluster& bot) {
		creator = &bot;
		const nlohmann::json& dbconf = config::get("database");
		if (!db::connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"], dbconf.contains("socket") ? dbconf["socket"] : "")) {
			creator->log(dpp::ll_critical, fmt::format("Database connection error connecting to {}: {}", dbconf["database"], mysql_error(&connection)));
			exit(2);
		}
		std::thread([&]() {
			dpp::utility::set_thread_name("sql/coro");
			while (true) {
				cached_query_results qr;
				{
					std::unique_lock<std::mutex> queue_lock(query_queue_mtx);
					sql_worker_cv.wait(queue_lock, [] {
						return !sql_query_queue.empty();
					});
					if (sql_query_queue.empty()) {
						continue;
					}
					qr = std::move(sql_query_queue.front());
					sql_query_queue.pop();
				}
				/**
				 * If a transaction is waiting to be executed, fit it atomically into
				 * the queue here. The holds_transaction_lock is a thread_local variable
				 * which can only EVER be true on this thread at this time. Only threads
				 * where this is set to true may issue db::query() calls whilst the
				 * transaction_in_progress atomic bool is true. This prevents other threads
				 * running queries that end up inside the transaction.
				 */
				if (transaction_in_progress && transaction_function) {
					holds_transaction_lock = true;
					transaction_function();
					holds_transaction_lock = false;
					transaction_function = {};
				}
				resultset results{};
				if (!qr.format.empty()) {
					results = query(qr.format, qr.parameters);
				}
				if (qr.callback) {
					qr.callback(results);
				}
			}
		}).detach();
		creator->log(dpp::ll_info, fmt::format("Connected to database: {}", dbconf["database"]));
	}

	/**
	 * @brief Certain types of query are not supported in the binary protocol
	 * used for prepared statements. As such they must be executed 'raw', and
	 * unprepared. This is not exposed to the interface as we don't want users
	 * directly calling raw_query() and summoning Little Bobby Tables. Only
	 * queries which return no result set are supported.
	 *
	 * @return true query executed
	 */
	bool raw_query(const std::string& query) {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return mysql_real_query(&connection, query.c_str(), query.length()) == 0;
	}

	bool start_transaction() {
		return raw_query("START TRANSACTION");
	}

	bool commit() {
		return raw_query("COMMIT");
	}

	bool rollback() {
		return raw_query("ROLLBACK");
	}

	void transaction(std::function<bool()> closure, sql_query_callback callback) {

		if (transaction_in_progress) {
			throw std::runtime_error("Transaction already in progress");
		}

		/**
		 * Set up transaction function, this is called when the queue is empty
		 * and the atomic bool transaction_in_progress is set.
		 */
		transaction_function = [closure](){
			try {
				start_transaction();
				bool should_commit{false};
				if (closure) {
					should_commit = closure();
				}
				if (should_commit) {
					commit();
				} else {
					rollback();
				}
			}
			catch (...) {
				rollback();
			}
			/**
			 * Re-enable the SQL queue so that queries can happen again
			 */
			transaction_in_progress = false;
		};

		/**
		 * Set the atomic bool that indicates a transaction should be executed.
		 * The transaction is inserted into the stream of SQL queries inside the
		 * SQL thread as one atomic operation. We also have to send a blank callback
		 * query to signal the condition variable and make the SQL queue advance.
		 */
		transaction_in_progress = true;
		query_callback("", {}, callback);
	}

#ifdef DPP_CORO
	dpp::async<resultset> co_transaction(std::function<bool()> closure) {
		return dpp::async<resultset>{ [closure] <typename C> (C &&cc) { return transaction(closure, std::forward<C>(cc)); }};
	}
#endif

	bool close() {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		mysql_close(&connection);
		for (const auto& cc : cached_queries) {
			mysql_stmt_close(cc.second.st);
			delete[] cc.second.bindings;
			delete[] cc.second.lengths;
		}
		cached_queries = {};
		mysql_library_end();
		return true;
	}

	const std::string& error() {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return last_error;
	}

	void log_error(const std::string& format, const std::string& error) {
		if (!format.empty()) {
			last_error = fmt::format("{} (query: {})", error, format);
			if (error == "Lost connection to MySQL server during query") {
				db::close();
				db::init(*creator);
			}
		} else {
			last_error = error;
		}
		creator->log(dpp::ll_error, last_error);
	}

	size_t affected_rows() {
		std::lock_guard<std::mutex> db_lock(db_mutex);
		return rows_affected;
	}

	resultset query(const std::string &format, const paramlist &parameters, double lifetime) {
		double now = dpp::utility::time_f();
		cached_query_results r{ .format = format, .parameters = parameters, .callback = nullptr };
		auto f = cached_query_res.find(r);
		if (f != cached_query_res.end()) {
			if (now < f->second.expiry) {
				return f->second.results;
			}
			cached_query_res.erase(f);
		}
		cached_query_result_set rs{ .results = query(format, parameters), .expiry = now + lifetime };
		cached_query_res.emplace(r, rs);
		return rs.results;
	}

	resultset query(const std::string &format, const paramlist &parameters) {

		/**
		 * If any thread except the queue thread attempts to run a synchronous query whilst
		 * a transaction is running, it must wait until the transaction completes.
		 */
		while (transaction_in_progress && !holds_transaction_lock) {
			std::this_thread::sleep_for(1ms);
		}

		/**
		 * One DB handle can't query the database from multiple threads at the same time.
		 * To prevent corruption of results, put a lock guard on queries.
		 */
		std::lock_guard<std::mutex> db_lock(db_mutex);
		resultset rv;

		if (mysql_ping(&connection)) {
			creator->log(dpp::ll_error, "SQL: Connection has died, reconnecting...");
			for (const auto& cc : cached_queries) {
				mysql_stmt_close(cc.second.st);
				delete[] cc.second.bindings;
				delete[] cc.second.lengths;
			}
			cached_queries = {};
			const nlohmann::json& dbconf = config::get("database");
			if (!db::unsafe_connect(dbconf["host"], dbconf["username"], dbconf["password"], dbconf["database"], dbconf["port"], dbconf.contains("socket") ? dbconf["socket"] : "")) {
				creator->log(dpp::ll_critical, fmt::format("Database connection error connecting to {}: {}", dbconf["database"], mysql_error(&connection)));
				return rv;
			}
		}

		/**
		 * Clear error status
		 */
		last_error.clear();
		/**
		 * Clear number of affected rows
		 */
		rows_affected = 0;

		++query_total;

		/**
		 * Check for a cached query in the query cache, if one is found, we can use it,
		 * and we don't need to call mysql_stmt_init() and mysql_stmt_prepare().
		 */
		cached_query cc;
		auto f = cached_queries.find(format);
		if (f != cached_queries.end()) {
			/* Query already exists in prepared statement cache */
			cc = f->second;
		} else {

			/* Query doesn't exist yet, initialise a prepared statement and allocate char buffers */
			cc.st = mysql_stmt_init(&connection);
			if (mysql_stmt_prepare(cc.st, format.c_str(), format.length())) {
				log_error(format, mysql_stmt_error(cc.st));
				rv.error = mysql_stmt_error(cc.st);
				mysql_stmt_close(cc.st);
				return rv;
			}

			/* Check the parameter count provided matches that which MySQL expects */
			size_t expected_param_count = mysql_stmt_param_count(cc.st);
			if (parameters.size() != expected_param_count) {
				rv.error = "Incorrect number of parameters: " + format + " (" + std::to_string(parameters.size()) + " vs " + std::to_string(expected_param_count) + ")";
				log_error(format, rv.error);
				mysql_stmt_close(cc.st);
				return rv;
			}

			/* Allocate memory for awful C stuff 🐉 */
			cc.bindings = nullptr;
			cc.lengths = nullptr;
			if (expected_param_count) {
				cc.bindings = new MYSQL_BIND[expected_param_count];
				cc.lengths = new unsigned long[expected_param_count];
				memset(cc.lengths, 0, sizeof(unsigned long) * expected_param_count);
			}

			/* Determine if this query expects results by the first keyword */
			std::vector<std::string> q = (dpp::utility::tokenize(dpp::trim(dpp::lowercase(format)), " "));
			cc.expects_results = (q.size() > 0 && (q[0] == "select" || q[0] == "show" || q[0] == "describe" || q[0] == "explain" || q[0] == "exists"));

			/* Store to cache */
			cached_queries.emplace(format, cc);
			creator->log(dpp::ll_debug, "SQL: New cached prepared statement: " + format);
		}

		if (parameters.size()) {

			/* Parameters are expected for this query, bind them to the prepared statement */
			memset(cc.bindings, 0, sizeof(*cc.bindings));
			cc.bufs.clear();
			cc.bufs.reserve(parameters.size());
			int v = 0;
			for (const auto& param : parameters) {
				std::visit([&cc, &v](auto &&p) {
					using T = std::decay_t<decltype(p)>;
					if constexpr (std::is_same_v<T, std::string>) {
						cc.bufs.emplace_back(p);
					} else {
						cc.bufs.emplace_back(std::to_string(p));
					}
					cc.lengths[v] = cc.bufs[v].length();
					cc.bindings[v].buffer_type = MYSQL_TYPE_VAR_STRING;
					cc.bindings[v].buffer = const_cast<char*>(cc.bufs[v].c_str());
					cc.bindings[v].buffer_length = cc.bufs[v].length() + 1;
					cc.bindings[v].is_null = nullptr;
					cc.bindings[v].length = &cc.lengths[v];
					v++;
				}, param);
			}

			/* Bind parameters to statement */
			if (mysql_stmt_bind_param(cc.st, (MYSQL_BIND*)cc.bindings)) {
				log_error(format, mysql_stmt_error(cc.st));
				rv.error = mysql_stmt_error(cc.st);
				return rv;
			}
		}

		int result{0};

		if (!cc.expects_results) {
			/**
			 * Query which does not expect results, e.g. UPDATE, INSERT
			 */
			result = mysql_stmt_execute(cc.st);
			if (result) {
				log_error(format, mysql_stmt_error(cc.st));
				rv.error = mysql_stmt_error(cc.st);
			} else {
				rv.affected_rows = rows_affected = mysql_stmt_affected_rows(cc.st);
			}
		} else {
			/**
			 * Query which expects results, e.g. SELECT
			 */
			unsigned long field_count{0};
			MYSQL_RES *a_res = mysql_stmt_result_metadata(cc.st);
			if (a_res) {
				field_count = mysql_stmt_field_count(cc.st);
				MYSQL_FIELD *fields = mysql_fetch_fields(a_res);
				MYSQL_BIND bindings[field_count];
				char* string_buffers[field_count];
				unsigned long lengths[field_count];
				bool is_null[field_count];
				std::memset(bindings, 0, sizeof(bindings));
				std::memset(is_null, 0, sizeof(is_null));
				std::memset(lengths, 0, sizeof(lengths));
				for (unsigned long i = 0; i < field_count; ++i) {
					/* FIX: Cap max length of retrieved field at 128k, else it will try and new[]
					 * a 4gb buffer, which is really REALLY slow!
					 */
					fields[i].length = std::min(fields[i].length, 131072UL);
					string_buffers[i] = new char[fields[i].length];
					memset(string_buffers[i], 0, fields[i].length);
					bindings[i].buffer_type = MYSQL_TYPE_VAR_STRING;
					bindings[i].buffer = string_buffers[i];
					bindings[i].buffer_length = fields[i].length;
					bindings[i].is_null = reinterpret_cast<my_bool*>(is_null[i] ? 49 : 48);
					bindings[i].length = &lengths[i];
				}

				result = mysql_stmt_bind_result(cc.st, (MYSQL_BIND*)&bindings);
				if (result) {
					log_error(format, mysql_stmt_error(cc.st));
					for (unsigned long i = 0; i < field_count; ++i) {
						delete[] string_buffers[i];
					}
					mysql_free_result(a_res);
					return rv;
				}

				result = mysql_stmt_execute(cc.st);
				if (result == 0) {

					/* Build resultset */
					while (true) {
						result = mysql_stmt_fetch(cc.st);
						if (result == MYSQL_NO_DATA) {
							/* End of resultset */
							break;
						} else if (result != 0) {
							/* Error retrieving resultset, e.g. disconnected */
							log_error(format, mysql_stmt_error(cc.st));
							break;
						}

						/* Build row*/
						if (mysql_num_fields(a_res)) {
							long s_field_count = 0;
							db::row thisrow;
							while (s_field_count < mysql_num_fields(a_res)) {
								std::string a = (fields[s_field_count].name ? fields[s_field_count].name : "");
								std::string b = is_null[s_field_count] ? "" : std::string(string_buffers[s_field_count], lengths[s_field_count]);
								thisrow[a] = b;
								s_field_count++;
							}
							rv.emplace_back(thisrow);
						}
					}
				}
				mysql_free_result(a_res);
				for (unsigned long i = 0; i < field_count; ++i) {
					delete[] string_buffers[i];
				}
			} else {
				log_error(format, mysql_stmt_error(cc.st));
				rv.error = mysql_stmt_error(cc.st);
			}
		}

		return rv;
	}
};