#pragma once

#include <dpp/dpp.h>

#define CUSTOM_ID_SPACER "###"

/**
 * Handler to attach functions to a specific component to reply later when the component get triggered.
 * The handler will hold added components for 10 minutes
 */
namespace ButtonHandler {

	/**
	 * Holds the callback for a stored component which will be executed later when a component is triggered
	 */
	struct Session {
		time_t created_at;
		std::function<bool(const dpp::button_click_t &)> function;
		/// Duration in seconds to hold the session in the cache
		uint16_t cache_duration;

		Session() : created_at(time(nullptr)), cache_duration(10) {}

		/**
		 * Whether the callback is expired and can be removed from the cache
		 */
		bool isExpired() const {
			return difftime(time(nullptr), created_at) > cache_duration * 60;
		}
	};

	/// cached components with their callbacks. Identified by the custom_id
	std::unordered_map<uint64_t, Session> cachedSessions;
	/// cache mutex
	std::shared_mutex cachedSessionsMutex;
	/// incrementing custom_id counter
	uint64_t customIdCounter;

	/**
	 * Remove too old sessions from the cache
	 */
	void clearGarbage() {
		std::unique_lock l(cachedSessionsMutex);

		auto it = cachedSessions.begin();
		while (it != cachedSessions.end()) {
			if (it->second.isExpired()) {
				assert(!cachedSessions.empty());
				it = cachedSessions.erase(it);  // <-- Return value should be a valid iterator.
			} else {
				++it;  // Have to manually increment.
			}
		}
	}

	/**
     * Set a callback to respond to a component.
     * The function will overwrite the custom_id of the component!
     * Call this BEFORE you add the component to the message!
     * @param component component to execute the function for when its triggered. The custom_id field gets overwritten!
     * @param function callback to execute when the component got triggered.
     * Return true if the callback should be removed from the cache after execution.
     * The handler will then no longer respond to the button. Otherwise return false
     * @param cache_duration Duration in minutes to hold the component in the cache. Defaults to 10 minutes
     */
	void bind(dpp::component &component, const std::function<bool(const dpp::button_click_t &)> &function, uint16_t cache_duration = 10) {
		clearGarbage();

		std::unique_lock l(cachedSessionsMutex);

		bool customIdAlreadyExists;
		do {
			if (customIdCounter >= UINT_LEAST64_MAX) {
				customIdCounter = 0;
			}
			customIdCounter++;

			customIdAlreadyExists = cachedSessions.find(customIdCounter) != cachedSessions.end();
			if (!customIdAlreadyExists) {
				component.custom_id = std::to_string(customIdCounter); // overwrite the custom_id from the given component

				Session session;
				session.function = function;
				session.cache_duration = cache_duration;
				component.custom_id += CUSTOM_ID_SPACER + std::to_string(static_cast<long int>(session.created_at)); // add creation time to the custom_id
				cachedSessions[customIdCounter] = session;
				customIdAlreadyExists = false; // break
			}
		} while (customIdAlreadyExists);
	}

	/**
 	 * call this in dpp::cluster::on_button_click
     * @param event the dpp::button_click_t event
     */
	void handle(const dpp::button_click_t &event) {
		// parse id and creation time from the event's custom_id
		uint64_t customId;
		time_t creationTimestamp;
		try {
			std::string id = event.custom_id.substr(0, event.custom_id.find(CUSTOM_ID_SPACER));
			std::string creation = event.custom_id.substr(
					event.custom_id.find(CUSTOM_ID_SPACER) + std::strlen(CUSTOM_ID_SPACER),
					std::string::npos
			);
			customId = std::stoul(id);
			creationTimestamp = std::stol(creation);
		} catch (std::out_of_range &e) {
			event.reply(dpp::message("invalid component").set_flags(dpp::m_ephemeral));
			return;
		} catch (std::invalid_argument &e) {
			event.reply(dpp::message("invalid component").set_flags(dpp::m_ephemeral));
			return;
		}

		std::unique_lock l(cachedSessionsMutex);

		auto existing = cachedSessions.find(customId);

		if (existing != cachedSessions.end() && existing->second.created_at == creationTimestamp && !existing->second.isExpired()) {
			bool forget = existing->second.function(event);
			if (forget) {
				cachedSessions.erase(existing);
			}
		}
	}

}