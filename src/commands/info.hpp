#include <dpp/dpp.h>
#include "../JsonFile.h"

using namespace std::literals::chrono_literals;

void handle_info(dpp::cluster& bot, const dpp::slashcommand_t& event, JsonFile& muteCounter) {
	uint32_t monthlyMutes = 0;
	uint32_t weeklyMutes = 0;
	// compute scores and clear muteCounter if needed
	std::vector<uint32_t> to_remove;
	{
		shared_lock l(muteCounter.get_mutex());
		uint32_t i = 0;
		for (uint32_t timestamp : muteCounter.content) {
			if (timestamp > time(nullptr) - 30 * 24 * 60 * 60) {
				monthlyMutes++;
				if (timestamp > time(nullptr) - 7 * 24 * 60 * 60) {
					weeklyMutes++;
				}
			} else {
				to_remove.push_back(i);
			}
			i++;
		}
	}
	for (auto i : to_remove) {
		muteCounter.content.erase(muteCounter.content.begin() + i);
	}
	muteCounter.save();


	event.reply(
			dpp::message().add_embed(
					dpp::embed()
							.set_color(dpp::colors::yellow)
							.set_title("Antispambot Information")
							.set_url("https://github.com/Commandserver/Antispambot")
							.set_description("A Discord Bot to protect your server from spam, invitations, fake nitro ads and more written in C++")
							.add_field("Mitigations this month", to_string(monthlyMutes), true)
							.add_field("Mitigations this week", to_string(weeklyMutes), true)
							.add_field("Uptime", bot.uptime().to_string(), true)
							.add_field("Online Users", to_string(dpp::get_user_count()), true)
							.add_field("Library Version", fmt::format("[{}]({})", DPP_VERSION_TEXT, "https://github.com/brainboxdotcc/DPP"), true)
							.set_footer("Sourcecode available on GitHub", "https://github.com/fluidicon.png")
			)
	);
}