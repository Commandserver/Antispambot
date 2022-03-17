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
		m.set_description("Bot Einstellungen");
		m.disable_default_permissions();

		// attach permissions to the slash command
		for (auto &id: config["admin-user-ids"]) {
			auto perms = dpp::command_permission();
			perms.id = id;
			perms.type = dpp::cpt_user;
			perms.permission = true;
			m.add_permission(perms);
		}

		m.add_option(dpp::command_option(dpp::co_sub_command_group, "domainblacklist", "Verwalte verbotene Domains")
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Füge eine Domain der Blacklist hinzu")
												 .add_option(dpp::command_option(dpp::co_string, "domain",
																				 "Die Domain, Top-Level-Domain oder Url",
																				 true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Entferne eine Domain aus der Blacklist")
												 .add_option(dpp::command_option(dpp::co_string, "domain",
																				 "Die Domain, Top-Level-Domain oder Url",
																				 true))
							 )
		);
		m.add_option(dpp::command_option(dpp::co_sub_command_group, "forbiddenwords", "Verwalte verbotene Wörter")
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Verbiete ein Wort")
												 .add_option(dpp::command_option(dpp::co_string, "word",
																				 "Das Wort das verboten werden soll",
																				 true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Entferne ein Wort aus der Schimpfwörter Bibliothek")
												 .add_option(dpp::command_option(dpp::co_string, "word",
																				 "Das Wort das aus der Liste entfernt werden soll",
																				 true))
							 )
		);
		m.add_option(dpp::command_option(dpp::co_sub_command_group, "bypass",
										 "Verwalte Rollen und User die von den Anti-Spam-Maßnahmen ausgeschlossen sind")
							 .add_option(dpp::command_option(dpp::co_sub_command, "list",
															 "Liste all Rollen und User auf die vom Anti-Spam-System ausgeschlossen sind")
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Füge Rollen oder User hinzu welche von den Anti-Spam-Maßnahmen ausgeschlossen sein sollen")
												 .add_option(dpp::command_option(dpp::co_mentionable, "mentionable",
																				 "Rolle oder User", true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Entferne Rollen oder User welche von den Anti-Spam-Maßnahmen wieder betroffen sein sollen")
												 .add_option(dpp::command_option(dpp::co_mentionable, "mentionable",
																				 "Rolle oder User", true))
							 )
		);
		m.add_option(dpp::command_option(dpp::co_sub_command_group, "extensionblacklist",
										 "Verwalte verbotene Datei-endungen")
							 .add_option(dpp::command_option(dpp::co_sub_command, "add",
															 "Füge verbotene Datei-endungen zur Blacklist hinzu")
												 .add_option(dpp::command_option(dpp::co_string, "file_extension",
																				 "Discord Server ID", true))
							 )
							 .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															 "Entferne verbotene Datei-endungen von der Blacklist")
												 .add_option(dpp::command_option(dpp::co_string, "file_extension",
																				 "Discord Server ID", true))
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