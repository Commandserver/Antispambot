#include <dpp/dpp.h>

dpp::slashcommand definition_info() {
	return dpp::slashcommand()
			.set_name("info")
			.set_description("Information about this bot");
}

void handle_info(dpp::cluster& bot, const dpp::slashcommand_t& event) {
	event.reply(
			dpp::message().add_embed(
					dpp::embed()
							.set_color(dpp::colors::yellow)
							.set_title("Antispambot Information")
							.set_url("https://github.com/Commandserver/Antispambot")
							.set_description("A Discord Bot to protect your server from spam, invitations, fake nitro ads and more written in C++")
							.add_field("Uptime", bot.uptime().to_string(), true)
							.add_field("Users", std::to_string(dpp::get_user_count()), true)
							.add_field("Servers", std::to_string(dpp::get_guild_count()), true)
							.add_field("Library Version", fmt::format("[{}]({})", dpp::utility::version(), "https://github.com/brainboxdotcc/DPP"), true)
							.set_footer("Project available on GitHub", "https://github.com/fluidicon.png")
			)
	);
}