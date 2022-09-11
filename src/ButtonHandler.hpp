#pragma once

#include <dpp/dpp.h>

#ifndef COMPONENT_CACHE_DURATION
#define COMPONENT_CACHE_DURATION 60 * 10
#endif

/**
 * Holds the callback for a stored component which will be executed later when a component is triggered
 */
struct ButtonContainer {
	std::function<void(const dpp::button_click_t &)> function;
	time_t created_at;
	bool only_one;

	ButtonContainer() : created_at(time(nullptr)), only_one(true) {}

	/**
	 * Whether the callback is expired and can be removed from the cache
	 */
	bool isExpired() const {
		return difftime(time(nullptr), created_at) > COMPONENT_CACHE_DURATION;
	}
};

/**
 * A static handler to attach functions to a specific component to reply later when the component get triggered.
 * The handler will hold added components for 10 minutes
 */
class ButtonHandler {
	/// cached components with their callbacks. Identified by the custom_id
	static std::unordered_map<uint64_t, ButtonContainer> cachedActions;
	/// cache mutex
	static std::shared_mutex cachedActionsMutex;
	/// incrementing custom_id counter
	static uint64_t customIdCounter;
public:
	/**
     * Set a callback to respond to a component. The function will overwrite the custom_id of the component!
     * @param component component to execute the function for when its triggered. It'll set the custom_id of the component to its internal counter
     * @param function callback to execute when the component got triggered
     * @param only_one If true, the callback can be called only once and will be then removed from the cache
     */
	static void bind(dpp::component &component, const std::function<void(const dpp::button_click_t &)> &function, bool only_one = true) {
		std::unique_lock l(cachedActionsMutex);

		// remove too old ones
		auto it = cachedActions.begin();
		while (it != cachedActions.end()) {
			if (it->second.isExpired()) {
				assert(!cachedActions.empty());
				it = cachedActions.erase(it);  // <-- Return value should be a valid iterator.
			} else {
				++it;  // Have to manually increment.
			}
		}

		bool customIdAlreadyExists;
		do {
			if (customIdCounter >= UINT_LEAST64_MAX) {
				customIdCounter = 0;
			}
			customIdCounter++;

			customIdAlreadyExists = cachedActions.find(customIdCounter) != cachedActions.end();
			if (!customIdAlreadyExists) {
				component.custom_id = std::to_string(customIdCounter); // overwrite the custom_id from the given component

				ButtonContainer container;
				container.function = function;
				container.only_one = only_one;
				cachedActions[customIdCounter] = container;
				customIdAlreadyExists = false; // break
			}
		} while (customIdAlreadyExists);
	}

	/**
 	 * call this in cluster::on_button_click
     * @param event the dpp::button_click_t event
     */
	static void call(const dpp::button_click_t &event) {
		std::unique_lock l(cachedActionsMutex);

		auto existing = cachedActions.find(std::stoull(event.custom_id));

		if (existing != cachedActions.end()) {
			existing->second.function(event);
			if (existing->second.only_one) {
				cachedActions.erase(existing);
			}
		}
	}

};

std::unordered_map<uint64_t, ButtonContainer> ButtonHandler::cachedActions;
std::shared_mutex ButtonHandler::cachedActionsMutex;
uint64_t ButtonHandler::customIdCounter;