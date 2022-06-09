#include <spdlog/spdlog.h>
#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>

using namespace std;

namespace commands {
	/**
	 * Create all guild slash commands
	 */
	void createAll(dpp::cluster &bot, nlohmann::json &config) {
		dpp::slashcommand m;
		m.set_name("manage");
		m.set_description("Bot Settings");
		m.set_default_permissions(0);

		m.add_option(dpp::command_option(dpp::co_sub_command_group, "domainblacklist", "Configure forbidden domains")
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Add a domain to the blacklist")
												 .add_option(dpp::command_option(dpp::co_string, "domain",
																				 "The domain, top-level-domain or the whole URL",
																				 true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Remove a domain from the blacklist")
												 .add_option(dpp::command_option(dpp::co_string, "domain",
																				 "The domain, top-level-domain or the whole URL",
																				 true))
							 )
		);
		m.add_option(dpp::command_option(dpp::co_sub_command_group, "forbiddenwords", "Configure bad words")
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Add a word to the list")
												 .add_option(dpp::command_option(dpp::co_string, "word",
																				 "The bad word",
																				 true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Remove a word from the list")
												 .add_option(dpp::command_option(dpp::co_string, "word",
																				 "The bad word",
																				 true))
							 )
		);
		m.add_option(dpp::command_option(dpp::co_sub_command_group, "bypass",
										 "Configure roles and users who are excluded from the anti-spam system")
							 .add_option(dpp::command_option(dpp::co_sub_command, "list",
															 "List all excluded roles and users")
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Add a role or user to be excluded from the anti-spam system")
												 .add_option(dpp::command_option(dpp::co_mentionable, "mentionable",
																				 "A role or user", true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Remove a role or user from the bypass-list")
												 .add_option(dpp::command_option(dpp::co_mentionable, "mentionable",
																				 "A role or user", true))
							 )
		);

		bot.guild_command_create(m, config["guild-id"], [&bot](const dpp::confirmation_callback_t &event) {
			if (event.is_error()) {
				bot.log(dpp::ll_error, "error creating slash commands: " + event.http_info.body);
			} else {
				bot.log(dpp::ll_info, "success creating slash commands");
			}
		});
	}
}