#include <dpp/dpp.h>

dpp::slashcommand definition_manage() {
	return dpp::slashcommand()
	.set_name("manage")
	.set_description("Bot Einstellungen")
	.set_default_permissions(0)

	.add_option(dpp::command_option(dpp::co_sub_command_group, "domainblacklist", "Verwalte verbotene Domains")
							  .add_option(dpp::command_option(dpp::co_sub_command, "add",
															  "Füge eine Domain der Blacklist hinzu")
												  .add_option(dpp::command_option(dpp::co_string, "domain","Die Domain, Top-Level-Domain oder Url",true))
							  )
							  .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															  "Entferne eine Domain aus der Blacklist")
												  .add_option(dpp::command_option(dpp::co_string, "domain","Die Domain, Top-Level-Domain oder Url",true))
							  )
	)
	.add_option(dpp::command_option(dpp::co_sub_command_group, "forbiddenwords", "Verwalte verbotene Wörter")
							  .add_option(dpp::command_option(dpp::co_sub_command, "add",
															  "Verbiete ein Wort")
												  .add_option(dpp::command_option(dpp::co_string, "word","Das Wort das verboten werden soll",true))
							  )
							  .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															  "Entferne ein Wort aus der Schimpfwörter Bibliothek")
												  .add_option(dpp::command_option(dpp::co_string, "word","Das Wort das aus der Liste entfernt werden soll",true))
							  )
	)
	.add_option(dpp::command_option(dpp::co_sub_command_group, "bypass",
										  "Verwalte Rollen und User die von den Anti-Spam-Maßnahmen ausgeschlossen sind")
							  .add_option(dpp::command_option(dpp::co_sub_command, "list",
															  "Liste all Rollen und User auf die vom Anti-Spam-System ausgeschlossen sind")
							  )
							  .add_option(dpp::command_option(dpp::co_sub_command, "add",
															  "Füge Rollen oder User hinzu welche von den Anti-Spam-Maßnahmen ausgeschlossen sein sollen")
												  .add_option(dpp::command_option(dpp::co_mentionable, "mentionable","Rolle oder User", true))
							  )
							  .add_option(dpp::command_option(dpp::co_sub_command, "remove",
															  "Entferne Rollen oder User welche von den Anti-Spam-Maßnahmen wieder betroffen sein sollen")
												  .add_option(dpp::command_option(dpp::co_mentionable, "mentionable","Rolle oder User", true))
							  )
	);
}

void handle_manage(dpp::cluster& bot, const dpp::slashcommand_t& event, ConfigSet& domainBlacklist, ConfigSet& forbiddenWords, ConfigSet& bypassConfig) {
	dpp::command_interaction cmd_data = event.command.get_command_interaction();

	if (cmd_data.options[0].name == "domainblacklist" and !cmd_data.options[0].options[0].options.empty()) {
		if (cmd_data.options[0].options[0].name == "add") {
			std::string domain = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);

			std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
			std::smatch domain_matches;
			if (regex_match(domain, domain_matches, domain_pattern) and domain_matches.size() == 2) {
				domain = domain_matches[1];
			} else if (!regex_match(domain, std::regex(R"(^\S*?\.[A-z]+$)"))) {
				event.reply(dpp::message("`" + domain + "` ist keine gültige Domain :x:").set_flags(dpp::m_ephemeral));
				return;
			}
			transform(domain.begin(), domain.end(), domain.begin(), ::tolower); // to lowercase

			if (domainBlacklist.contains(domain)) {
				event.reply(dpp::message("`" + domain + "` ist schon auf der Blacklist").set_flags(dpp::m_ephemeral));
				return;
			}

			domainBlacklist.insert(domain);
			domainBlacklist.save();

			event.reply(dpp::message("`" + domain + "` wurde der Blacklist hinzugefügt :white_check_mark:").set_flags(dpp::m_ephemeral));
		} else if (cmd_data.options[0].options[0].name == "remove") {
			std::string domain = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);

			std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
			std::smatch domain_matches;
			if (regex_match(domain, domain_matches, domain_pattern) and domain_matches.size() == 2) {
				domain = domain_matches[1];
			} else if (!regex_match(domain, std::regex(R"(^\S*?\.[A-z]+$)"))) {
				event.reply(dpp::message("`" + domain + "` ist keine gültige Domain :x:").set_flags(dpp::m_ephemeral));
				return;
			}
			transform(domain.begin(), domain.end(), domain.begin(), ::tolower); // to lowercase

			if (!domainBlacklist.contains(domain)) {
				event.reply(dpp::message("`" + domain + "` ist nicht auf der Blacklist").set_flags(dpp::m_ephemeral));
				return;
			}

			domainBlacklist.remove(domain);
			domainBlacklist.save();

			event.reply(dpp::message("`" + domain + "` wurde von der Blacklist entfernt :white_check_mark:").set_flags(dpp::m_ephemeral));
		}
	} else if (cmd_data.options[0].name == "forbiddenwords" and !cmd_data.options[0].options[0].options.empty()) {
		if (cmd_data.options[0].options[0].name == "add") {
			std::string word = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);
			transform(word.begin(), word.end(), word.begin(), ::tolower); // to lowercase

			if (forbiddenWords.contains(word)) {
				event.reply(dpp::message("`" + word + "` ist schon in der Schimpfwörter Bibliothek :white_check_mark:").set_flags(dpp::m_ephemeral));
				return;
			}

			forbiddenWords.insert(word);
			forbiddenWords.save();

			event.reply(dpp::message("`" + word + "` wurde zur Schimpfwörter Bibliothek hinzugefügt :white_check_mark:").set_flags(dpp::m_ephemeral));
		} else if (cmd_data.options[0].options[0].name == "remove") {
			std::string word = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);
			transform(word.begin(), word.end(), word.begin(), ::tolower); // to lowercase

			if (!forbiddenWords.contains(word)) {
				event.reply(dpp::message("`" + word + "` ist nicht in der Schimpfwörter Bibliothek :x:").set_flags(dpp::m_ephemeral));
				return;
			}

			forbiddenWords.remove(word);
			forbiddenWords.save();

			event.reply(dpp::message("`" + word + "` wurde aus der Schimpfwörter Bibliothek entfernt :white_check_mark:").set_flags(dpp::m_ephemeral));
		}
	} else if (cmd_data.options[0].name == "bypass") {
		if (cmd_data.options[0].options[0].name == "add" and !cmd_data.options[0].options[0].options.empty()) {
			auto id = std::get<dpp::snowflake>(cmd_data.options[0].options[0].options[0].value);

			std::string mention;
			if (dpp::find_role(id)) {
				mention = "<@&" + std::to_string(id) + ">";
			} else {
				mention = "<@" + std::to_string(id) + ">";
			}

			if (bypassConfig.contains(std::to_string(id))) {
				event.reply(dpp::message(":white_check_mark: " + mention + " ist schon ausgeschlossen vom Anti-Spam-System").set_flags(dpp::m_ephemeral));
			} else {
				bypassConfig.insert(std::to_string(id));
				bypassConfig.save();

				event.reply(dpp::message(":white_check_mark: " + mention + " ist nun ausgeschlossen vom Anti-Spam-System").set_flags(dpp::m_ephemeral));
			}
		} else if (cmd_data.options[0].options[0].name == "remove" and !cmd_data.options[0].options[0].options.empty()) {
			auto id = std::get<dpp::snowflake>(cmd_data.options[0].options[0].options[0].value);

			std::string mention;
			if (dpp::find_role(id)) {
				mention = "<@&" + std::to_string(id) + ">";
			} else {
				mention = "<@" + std::to_string(id) + ">";
			}

			if (!bypassConfig.contains(std::to_string(id))) {
				event.reply(dpp::message(mention + " ist nicht ausgeschlossen").set_flags(dpp::m_ephemeral));
			} else {
				bypassConfig.remove(std::to_string(id));
				bypassConfig.save();

				event.reply(dpp::message(":white_check_mark: " + mention + " ist nicht mehr vom Anti-Spam-System ausgeschlossen").set_flags(dpp::m_ephemeral));
			}
		} else if (cmd_data.options[0].options[0].name == "list") {
			std::string userMentions;
			std::string roleMentions;

			// loop through the saved ids to check whether they're roles or users
			{
				std::shared_lock l(bypassConfig.get_mutex());
				for (auto &entry : bypassConfig.get_container()) {
					if (dpp::find_role(stol(entry))) {
						roleMentions += "<@&" + entry + ">\n";
					} else {
						userMentions += "<@" + entry + ">\n";
					}
				}
			}

			auto embed = dpp::embed()
					.set_description("Vom Anti-Spam-System ausgeschlossene Rollen und User");
			if (!userMentions.empty()) {
				embed.add_field("Bypassed users", userMentions);
			}
			if (!roleMentions.empty()) {
				embed.add_field("Bypassed roles", roleMentions);
			}
			if (userMentions.empty() and roleMentions.empty()) {
				event.reply(dpp::message("Niemand ist vom Anti-Spam-System ausgeschlossen").set_flags(dpp::m_ephemeral));
			} else {
				event.reply(dpp::message().set_flags(dpp::m_ephemeral).add_embed(embed));
			}
		}
	}
}