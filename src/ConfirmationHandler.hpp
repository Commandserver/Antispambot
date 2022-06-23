#pragma once
#include <dpp/dpp.h>

struct ComponentContainer {
	std::function<void(const dpp::button_click_t &)> function;
	time_t created_at;

	ComponentContainer() : created_at(time(nullptr)) {}
};


std::unordered_map<std::string, ComponentContainer> cachedActions;

uint64_t cachedActionsCounter = 0;

std::shared_mutex cachedActionsMutex;


void bindComponentAction(dpp::component &component, const std::function<void(const dpp::button_click_t &)>& function) {
	std::unique_lock l(cachedActionsMutex);

	{
		// remove too old ones
		auto it = cachedActions.begin();

		while (it != cachedActions.end()) {

			ComponentContainer container = it->second;
			const bool deleteEntry = difftime(time(nullptr), container.created_at) > 60 * 2;

			if (deleteEntry){
				assert(!cachedActions.empty());
				it = cachedActions.erase(it);  // <-- Return value should be a valid iterator.
			}
			else{
				++it;  // Have to manually increment.
			}
		}
	}

	cachedActionsCounter++;
	std::string id = component.custom_id + std::to_string(cachedActionsCounter); // create a unique custom_id for the component

	auto existing = cachedActions.find(id);
	if (existing == cachedActions.end()) {
		component.custom_id = id; // overwrite the custom_id from the given component
		ComponentContainer container;
		container.function = function;
		cachedActions[id] = container;
	}
}

void callComponent(const dpp::button_click_t &event, const std::string &custom_id) {
	std::shared_lock l(cachedActionsMutex);

	auto existing = cachedActions.find(custom_id);

	if (existing != cachedActions.end()) {
		std::cout << "call component callback with custom_id: " << custom_id << std::endl;
		existing->second.function(event);
	}
}

