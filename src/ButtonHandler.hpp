#pragma once
#include <dpp/dpp.h>
#include <random>

/**
 * Holds the callback for a stored component which will be executed when e.g. a button got clicked
 */
struct ComponentContainer {
	std::function<void(const dpp::button_click_t &)> function;
	time_t created_at;
	bool only_one;

	ComponentContainer() : created_at(time(nullptr)), only_one(true) {}
};


std::unordered_map<uint64_t, ComponentContainer> cachedActions;

std::shared_mutex cachedActionsMutex;


/**
 * Set a callback to respond to a component. The function will overwrite the custom_id of the component!
 * @param component component to execute the function for when its triggered
 * @param function callback to execute when the component got triggered
 * @param only_one If true, the callback will be called only once
 */
void bindComponentAction(dpp::component &component, const std::function<void(const dpp::button_click_t &)>& function, bool only_one = true) {
	std::unique_lock l(cachedActionsMutex);

	// remove too old ones
	auto it = cachedActions.begin();
	while (it != cachedActions.end()) {
		const bool deleteEntry = difftime(time(nullptr), it->second.created_at) > 60 * 10; // if older than 10 minutes

		if (deleteEntry) {
			assert(!cachedActions.empty());
			it = cachedActions.erase(it);  // <-- Return value should be a valid iterator.
		} else {
			++it;  // Have to manually increment.
		}
	}

	for (int i = 0; i < 100; i++) { // try 100 times
		// create a random custom_id for the component
		std::random_device rd;
		std::mt19937_64 gen(rd());
		std::uniform_int_distribution<uint64_t> dis;
		uint64_t custom_id = dis(gen);

		auto existing = cachedActions.find(custom_id);
		if (existing == cachedActions.end()) {
			component.custom_id = std::to_string(custom_id); // overwrite the custom_id from the given component

			ComponentContainer container;
			container.function = function;
			container.only_one = only_one;
			cachedActions[custom_id] = container;
			break;
		}
	}
}

/**
 * call this in cluster::on_button_click
 * @param event the dpp::button_click_t event
 */
void callComponent(const dpp::button_click_t &event) {
	std::unique_lock l(cachedActionsMutex);

	auto existing = cachedActions.find(std::stoull(event.custom_id));

	if (existing != cachedActions.end()) {
		std::cout << "call component callback with custom_id: " << event.custom_id << std::endl;
		existing->second.function(event);
		if (existing->second.only_one) {
			cachedActions.erase(existing);
		}
	}
}

