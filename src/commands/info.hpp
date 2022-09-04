#include <dpp/dpp.h>

dpp::slashcommand definition_info() {
	return dpp::slashcommand()
			.set_name("info")
			.set_description("Information about this bot");
}

void handle_info(dpp::cluster& bot, const dpp::slashcommand_t& event, JsonFile& muteCounter) {

	uint32_t monthlyMutes = 0;
	uint32_t weeklyMutes = 0;
	// compute scores and clear muteCounter if needed
	std::vector<uint32_t> to_remove;
	{
		std::shared_lock l(muteCounter.get_mutex());
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
							.add_field("Mitigations this month", std::to_string(monthlyMutes), true)
							.add_field("Mitigations this week", std::to_string(weeklyMutes), true)
							.add_field("Uptime", bot.uptime().to_string(), true)
							.add_field("Users", std::to_string(dpp::get_user_count()), true)
							.add_field("Library Version", fmt::format("[{}]({})", dpp::utility::version(), "https://github.com/brainboxdotcc/DPP"), true)
							.set_footer("Project available on GitHub", "https://github.com/fluidicon.png")
			)
	);
}