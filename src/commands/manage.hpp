#include <dpp/dpp.h>

dpp::slashcommand definition_config() {
	return dpp::slashcommand()
			.set_name("config")
			.set_description("Bot Settings")
			.set_default_permissions(0)

			.add_option(dpp::command_option(dpp::co_sub_command_group, "domainblacklist", "Configure forbidden domains")
								.add_option(dpp::command_option(dpp::co_sub_command, "add",
																"Add a domain to the blacklist")
													.add_option(dpp::command_option(dpp::co_string, "domain", "The domain, top-level-domain or the whole URL",true))
								)
								.add_option(dpp::command_option(dpp::co_sub_command, "remove",
																"Remove a domain from the blacklist")
													.add_option(dpp::command_option(dpp::co_string, "domain", "The domain, top-level-domain or the whole URL",true))
								)
			)
			.add_option(dpp::command_option(dpp::co_sub_command_group, "forbiddenwords", "Configure bad words")
								.add_option(dpp::command_option(dpp::co_sub_command, "add",
																"Add a word to the list")
													.add_option(dpp::command_option(dpp::co_string, "word", "The bad word",true))
								)
								.add_option(dpp::command_option(dpp::co_sub_command, "remove",
																"Remove a word from the list")
													.add_option(dpp::command_option(dpp::co_string, "word", "The bad word",true))
								)
			)
			.add_option(dpp::command_option(dpp::co_sub_command_group, "bypass",
											"Configure roles and users who are excluded from the anti-spam system")
								.add_option(dpp::command_option(dpp::co_sub_command, "list",
																"List all excluded roles and users")
								)
								.add_option(dpp::command_option(dpp::co_sub_command, "add",
																"Add a role or user to be excluded from the anti-spam system")
													.add_option(dpp::command_option(dpp::co_mentionable, "mentionable", "A role or user", true))
								)
								.add_option(dpp::command_option(dpp::co_sub_command, "remove",
																"Remove a role or user from the bypass-list")
													.add_option(dpp::command_option(dpp::co_mentionable, "mentionable", "A role or user", true))
								)
			);
}

void handle_config(const dpp::slashcommand_t& event, ConfigSet& domainBlacklist, ConfigSet& forbiddenWords, ConfigSet& bypassConfig) {
	dpp::command_interaction cmd_data = std::get<dpp::command_interaction>(event.command.data);

	if (cmd_data.options[0].name == "domainblacklist" and !cmd_data.options[0].options[0].options.empty()) {
		if (cmd_data.options[0].options[0].name == "add") {
			std::string domain = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);

			std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
			std::smatch domain_matches;
			if (regex_match(domain, domain_matches, domain_pattern) and domain_matches.size() == 2) {
				domain = domain_matches[1];
			} else if (!regex_match(domain, std::regex(R"(^\S*?\.[A-z]+$)"))) {
				event.reply(dpp::message("`" + domain + "` is not a valid domain :x:").set_flags(dpp::m_ephemeral));
				return;
			}
			transform(domain.begin(), domain.end(), domain.begin(), ::tolower); // to lowercase

			if (domainBlacklist.contains(domain)) {
				event.reply(dpp::message("`" + domain + "` is already on the blacklist").set_flags(dpp::m_ephemeral));
				return;
			}

			domainBlacklist.insert(domain);
			domainBlacklist.save();

			event.reply(dpp::message("`" + domain + "` was added to the blacklist :white_check_mark:").set_flags(dpp::m_ephemeral));
		} else if (cmd_data.options[0].options[0].name == "remove") {
			std::string domain = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);

			std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
			std::smatch domain_matches;
			if (regex_match(domain, domain_matches, domain_pattern) and domain_matches.size() == 2) {
				domain = domain_matches[1];
			} else if (!regex_match(domain, std::regex(R"(^\S*?\.[A-z]+$)"))) {
				event.reply(dpp::message("`" + domain + "` is not a valid domain :x:").set_flags(dpp::m_ephemeral));
				return;
			}
			transform(domain.begin(), domain.end(), domain.begin(), ::tolower); // to lowercase

			if (!domainBlacklist.contains(domain)) {
				event.reply(dpp::message("`" + domain + "` is not blacklisted").set_flags(dpp::m_ephemeral));
				return;
			}

			domainBlacklist.remove(domain);
			domainBlacklist.save();

			event.reply(dpp::message("`" + domain + "` was removed from the blacklist :white_check_mark:").set_flags(dpp::m_ephemeral));
		}
	} else if (cmd_data.options[0].name == "forbiddenwords" and !cmd_data.options[0].options[0].options.empty()) {
		if (cmd_data.options[0].options[0].name == "add") {
			std::string word = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);
			transform(word.begin(), word.end(), word.begin(), ::tolower); // to lowercase

			if (forbiddenWords.contains(word)) {
				event.reply(dpp::message("`" + word + "` is on the bad-words-list already :white_check_mark:").set_flags(dpp::m_ephemeral));
				return;
			}

			forbiddenWords.insert(word);
			forbiddenWords.save();

			event.reply(dpp::message("`" + word + "` was added to the bad-words-list :white_check_mark:").set_flags(dpp::m_ephemeral));
		} else if (cmd_data.options[0].options[0].name == "remove") {
			std::string word = std::get<std::string>(cmd_data.options[0].options[0].options[0].value);
			transform(word.begin(), word.end(), word.begin(), ::tolower); // to lowercase

			if (!forbiddenWords.contains(word)) {
				event.reply(dpp::message("`" + word + "` is not in the bad-words-list :x:").set_flags(dpp::m_ephemeral));
				return;
			}

			forbiddenWords.remove(word);
			forbiddenWords.save();

			event.reply(dpp::message("`" + word + "` was removed from the bad-words-list :white_check_mark:").set_flags(dpp::m_ephemeral));
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
				event.reply(dpp::message(":white_check_mark: " + mention + " is already excluded from the anti-spam system").set_flags(dpp::m_ephemeral));
			} else {
				bypassConfig.insert(std::to_string(id));
				bypassConfig.save();

				event.reply(dpp::message(":white_check_mark: " + mention + " got excluded from the anti-spam system").set_flags(dpp::m_ephemeral));
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
				event.reply(dpp::message(mention + " is not bypassed").set_flags(dpp::m_ephemeral));
			} else {
				bypassConfig.remove(std::to_string(id));
				bypassConfig.save();

				event.reply(dpp::message(":white_check_mark: " + mention + " is no more excluded from the anti-spam system").set_flags(dpp::m_ephemeral));
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
					.set_description("Roles and users excluded from the anti-spam system");
			if (!userMentions.empty()) {
				embed.add_field("Bypassed users", userMentions);
			}
			if (!roleMentions.empty()) {
				embed.add_field("Bypassed roles", roleMentions);
			}
			if (userMentions.empty() and roleMentions.empty()) {
				event.reply(dpp::message("There's no one bypassed").set_flags(dpp::m_ephemeral));
			} else {
				event.reply(dpp::message().set_flags(dpp::m_ephemeral).add_embed(embed));
			}
		}
	}
}