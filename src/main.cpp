#include <iostream>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <ctime>
#include <dpp/dpp.h>
#include <dpp/nlohmann/json.hpp>
#include <regex>
#include <algorithm>
#include <set>

#define MAX_DISCORD_FILE_SIZE 8000000

#include "classes/ButtonHandler.hpp"

#include "utils.hpp"
#include "classes/ConfigSet.h"
#include "classes/CachedGuildMember.h"

#include "commands/info.hpp"
#include "commands/manage.hpp"
#include "commands/massban.hpp"
#include "commands/masskick.hpp"

#define DAY 86400 //!< amount of seconds of a day
#define MINUTE 60 //!< amount of seconds of a minute

#define FAST_JOIN_THRESHOLD 60 //!< The time in seconds in which a certain number of players have to join, to be recognized as a raid. You can adjust this if you want
#define FAST_JOIN_MIN_MEMBER_COUNT 7 //!< When more than x members joined in the past (FAST_JOIN_THRESHOLD) seconds. You can adjust this number depending on your guild size


int main() {
	/* parse config */
	nlohmann::json config;
	std::ifstream configfile("../config.json");
	configfile >> config;
	configfile.close();


	/* create bot */
	dpp::cluster bot(config["token"], dpp::i_guilds | dpp::i_guild_messages | dpp::i_guild_members | dpp::i_message_content);


	const std::string log_name = config["log-filename"];

	/* Set up spdlog logger */
	std::shared_ptr<spdlog::logger> log;
	spdlog::init_thread_pool(8192, 2);
	std::vector<spdlog::sink_ptr> sinks;
	auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
	auto rotating = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_name, 1024 * 1024 * 5, 10);
	sinks.push_back(stdout_sink);
	sinks.push_back(rotating);
	log = std::make_shared<spdlog::async_logger>("logs", sinks.begin(), sinks.end(), spdlog::thread_pool(),
												 spdlog::async_overflow_policy::block);
	spdlog::register_logger(log);
	log->set_pattern("%^%Y-%m-%d %H:%M:%S.%e [%L] [th#%t]%$ : %v");
	log->set_level(spdlog::level::level_enum::info);

	/* Integrate spdlog logger to D++ log events */
	bot.on_log([&log](const dpp::log_t &event) {
		switch (event.severity) {
			case dpp::ll_trace:
				log->trace("{}", event.message);
				break;
			case dpp::ll_debug:
				log->debug("{}", event.message);
				break;
			case dpp::ll_info:
				log->info("{}", event.message);
				break;
			case dpp::ll_warning:
				log->warn("{}", event.message);
				break;
			case dpp::ll_error:
				log->error("{}", event.message);
				break;
			case dpp::ll_critical:
			default:
				log->critical("{}", event.message);
				break;
		}
	});

	/// Users and Roles IDs who are excluded from the spam detection
	ConfigSet bypassConfig("../bypass-config.txt");
	ConfigSet domainBlacklist("../domain-blacklist.txt");
	ConfigSet forbiddenWords("../bad-words.txt");


	bot.on_ready([&bot, &config](const dpp::ready_t &event) {
		bot.log(dpp::ll_info, "Logged in as " + bot.me.username + "! Using " + dpp::utility::version());

		if (dpp::run_once<struct register_bot_commands>()) {
			std::vector<dpp::slashcommand> commands = {
					definition_info(),
					definition_config(),
					definition_massban(),
					definition_masskick(),
			};

			bot.guild_bulk_command_create(commands, config["guild-id"].get<std::uint64_t>());
		}
	});


	bot.on_slashcommand([&domainBlacklist, &forbiddenWords, &bypassConfig, &bot](const dpp::slashcommand_t &event) -> dpp::task<void> {
		if (event.command.get_command_name() == "config") {
			handle_config(event, domainBlacklist, forbiddenWords, bypassConfig);
		} else if (event.command.get_command_name() == "info") {
			handle_info(bot, event);
		} else if (event.command.get_command_name() == "massban") {
			co_await handle_massban(bot, event);
		} else if (event.command.get_command_name() == "masskick") {
			co_await handle_masskick(bot, event);
		}
	});


	bot.on_button_click([](const dpp::button_click_t &event) -> dpp::task<void> {
		co_await ButtonHandler::handle(event);
	});


	// set a rate limit on all threads to prevent spam in threads
	const uint16_t thread_rate_limit = config["thread-rate-limit"].get<std::uint16_t>();
	if (thread_rate_limit > 0) {
		bot.on_thread_create([&bot, &thread_rate_limit](const dpp::thread_create_t &event) {
			auto t = event.created;
			t.rate_limit_per_user = thread_rate_limit;
			bot.channel_edit(t);
		});
	}


	/// Caches all joining members for raid detection
	dpp::cache<CachedGuildMember> guild_member_cache;

	/// usually empty. filled with members that are too fast joined
	dpp::cache<CachedGuildMember> fast_joined_members;

	time_t first_join = 0;

	bot.start_timer([&guild_member_cache, &first_join, &fast_joined_members, &config, &bot, &log](unsigned int timerId){

		int join_pause = 0; //!< seconds since the last join (excludes the current join event)

		/*
		 * garbage collection of guild_member_cache
		 */

		/* https://github.com/brainboxdotcc/DPP/blob/a7c9e02253707fa73f242d8c07b489d13f95e14a/src/dpp/discordclient.cpp#L514-L548 */
		/* a list of too old messages that could be removed */
		std::vector<CachedGuildMember *> to_remove;

		{
			// current utc time
			time_t t = time(nullptr);
			auto local_field = *gmtime(&t);
			auto utc = mktime(&local_field);

			/* IMPORTANT: We must lock the container to iterate it */
			std::shared_lock member_cache_lock(guild_member_cache.get_mutex());
			auto &member_cache_container = guild_member_cache.get_container();

			for (auto &[id, m]: member_cache_container) {
				int diff = (int) difftime(utc, m->gm.joined_at);

				if (diff > MINUTE * 10) { // check if the member is not joined in the past 10 minutes
					to_remove.push_back(m); // remove the member then from the cache
				}

				// join pause
				if ((join_pause == 0 or join_pause > diff)) {
					join_pause = diff;
				}
			}
			member_cache_lock.unlock();

			// remove too old guild members
			for (const auto mp: to_remove) {
				guild_member_cache.remove(mp); // will lock the container with unique mutex
			}
		}

		// when no users joined in the last 40 seconds, or 6 minutes since the first join exceeds
		if (fast_joined_members.count() > 0 and first_join and (difftime(time(nullptr), first_join) > MINUTE * 6 or join_pause > 40)) {
			log->info("raid detected detected, with " + std::to_string(fast_joined_members.count()) + " members");

			// send log message
			dpp::embed embed;
			embed.set_color(0xff0000);
			embed.set_timestamp(first_join);
			embed.set_title(fmt::format(":o: Raid detected with {} Users", fast_joined_members.count()));
			std::string file = "| User ID\t\t| Account creation\t| Joined\t\t| Username\n";
			dpp::guild_member *firstUser;
			dpp::guild_member *lastUser;
			{
				std::shared_lock l(fast_joined_members.get_mutex());
				auto &container = fast_joined_members.get_container();
				firstUser = &container.begin()->second->gm;
				lastUser = &container.begin()->second->gm;
				for (auto &[id, m] : container) {

					if (firstUser->joined_at > m->gm.joined_at) {
						firstUser = &m->gm;
					}
					if (lastUser->joined_at < m->gm.joined_at) {
						lastUser = &m->gm;
					}

					// add member to file
					auto creation = static_cast<time_t>(m->id.get_creation_time());
					file += std::to_string(m->id) + "\t" + formatTime(creation) + "\t" + formatTime(m->gm.joined_at);
					auto user = dpp::find_user(m->id);
					if (user) {
						file += "\t" + user->username;
					}
					file += "\n";
				}
			}

			embed.add_field("First user", fmt::format("User: {}\nJoined: {}\n\u200b", firstUser->get_mention(), dpp::utility::timestamp(firstUser->joined_at, dpp::utility::tf_long_time)), true);
			embed.add_field("Last user", fmt::format("User: {}\nJoined: {}\n\u200b", lastUser->get_mention(), dpp::utility::timestamp(lastUser->joined_at, dpp::utility::tf_long_time)), true);
			dpp::message msg("@everyone");
			msg.allowed_mentions.parse_everyone = true;
			msg.channel_id = config["log-channel-id"].get<std::uint64_t>();
			msg.add_embed(embed);
			msg.add_file("users.txt", file);
			bot.message_create(msg);

			{
				std::unique_lock l(fast_joined_members.get_mutex());
				auto &container = fast_joined_members.get_container();
				container.clear();
			}
			first_join = 0;
		}
	}, 10); // execute every 10 seconds

	bot.on_guild_member_add([&](const dpp::guild_member_add_t &event) {
		/* continue only on the correct guild */
		if (event.adding_guild->id != config["guild-id"].get<std::uint64_t>()) return;

		/* add the member to the cache */
		{
			/* Make a permanent pointer using new, for each message to be cached */
			auto *m = new CachedGuildMember();
			/* Store the message into the pointer by copying it */
			*m = static_cast<CachedGuildMember>(event.added);
			/* Store the new pointer to the cache using the store() method */
			guild_member_cache.store(m);
		}

		/// The amount of members who joined in the last 60 seconds
		uint32_t fast_joined_member_count = 0;

		// current utc time
		time_t t = time(nullptr);
		auto local_field = *gmtime(&t);
		auto utc = mktime(&local_field);

		/* IMPORTANT: We must lock the container to iterate it */
		std::shared_lock l(guild_member_cache.get_mutex());
		auto &member_cache_container = guild_member_cache.get_container();

		for (auto &[id, m] : member_cache_container) {
			int diff = (int) difftime(utc, m->gm.joined_at);

			if (diff < FAST_JOIN_THRESHOLD) { // count the amount of members who joined in the last 60 seconds
				fast_joined_member_count++;
			}
		}

		if (fast_joined_member_count >= FAST_JOIN_MIN_MEMBER_COUNT) {
			if (!first_join) {
				first_join = time(nullptr);
			}

			for (const auto &[id, m] : member_cache_container) {
				if (difftime(utc, m->gm.joined_at) < FAST_JOIN_THRESHOLD) {
					if (!fast_joined_members.find(id)) {
						/* Make a permanent pointer using new, for each message to be cached */
						auto *gm = new CachedGuildMember();
						/* Store the message into the pointer by copying it */
						*gm = static_cast<CachedGuildMember>(m->gm);
						/* Store the new pointer to the cache using the store() method */
						fast_joined_members.store(gm);
					}
				}
			}
		}
	});


	/// Holds all incoming messages for some time for the spam detection
	dpp::cache<dpp::message> message_cache;


	bot.on_message_create([&bot, &log, &message_cache, &domainBlacklist, &forbiddenWords, &config, &bypassConfig](const dpp::message_create_t &event) -> dpp::task<void> {
		/*
		 * Do nothing when:
		 * - from a bot
		 * - crossposted
		 * - from the system
		 * - is from the bot itself
		 * - not a normal message from a normal user
		 */
		if (event.msg.is_crosspost() or event.msg.is_crossposted() or event.msg.author.is_system() or
			event.msg.author.is_bot() or event.msg.author.id == bot.me.id or
			(event.msg.type != dpp::mt_default and event.msg.type != dpp::mt_reply and event.msg.type != dpp::mt_thread_starter_message)) {
			co_return;
		}

		// do nothing when whitelisted channel
		for (auto &id : config["excluded_channel_ids"]) {
			if (event.msg.channel_id == id.get<std::uint64_t>()) {
				co_return;
			}
		}
		// do nothing when whitelisted category
		auto *channel = dpp::find_channel(event.msg.channel_id);
		if (channel and channel->parent_id) {
			for (auto &id: config["excluded_category_ids"]) {
				if (channel->parent_id == id.get<std::uint64_t>()) {
					co_return;
				}
			}
		}

		// do nothing when whitelisted user or role
		{
			std::shared_lock l(bypassConfig.get_mutex());
			for (const std::string &entry: bypassConfig.get_container()) {
				if (event.msg.author.id == static_cast<dpp::snowflake>(entry)) {
					co_return;
				}
				for (dpp::snowflake roleId: event.msg.member.get_roles()) {
					if (roleId == static_cast<dpp::snowflake>(entry)) {
						co_return;
					}
				}
			}
		}

		/* add the message to the cache */
		{
			/* Make a permanent pointer using new, for each message to be cached */
			auto *m = new dpp::message();
			/* Store the message into the pointer by copying it */
			*m = event.msg;
			/* Store the new pointer to the cache using the store() method */
			message_cache.store(m);
		}

		/* begin of the main algorithm */

		/// The different discord invite codes of the message to resolve later
		std::set<std::string> inviteCodes;
		/// The total amount of invite urls in the message
		uint32_t inviteCount = 0;
		/// Total count of urls in the message
		uint32_t urlCount = 0;

		if (!event.msg.content.empty()) {

			// collect discord invitation codes (from urls and raw text)
			if (event.msg.content.find("discord") != std::string::npos) { // check the validity of the whole string
				std::smatch match;
				std::regex r(R"(discord(?:\.gg|(?:app)?\.com\/invite)\/(\S+))", std::regex_constants::icase);
				// loop through all invites in the message
				for (std::string s = event.msg.content; regex_search(s, match, r); s = match.suffix().str()) {
					if (match.size() == 2) { // should be always 2
						inviteCount++;
						inviteCodes.insert(match[1].str()); // store the invite code in a set
					}
				}
			}

			// setup url iterator
			std::regex url_pattern(R"(https?:\/\/\S+?\.(?:(?:\S+?(?=https?:\/\/))|\S+))", std::regex_constants::icase);
			auto words_begin = std::sregex_iterator(event.msg.content.begin(), event.msg.content.end(), url_pattern);
			auto words_end = std::sregex_iterator();
			// loop through all urls in the message
			for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
				urlCount++;
				const std::string URL = i->str();

				log->debug("URL: " + URL);

				// check if the domain or top-level-domain is blacklisted
				std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
				std::smatch domain_matches;
				if (regex_match(URL, domain_matches, domain_pattern)) {
					if (domain_matches.size() == 2) { // should be always 2
						std::string domain = domain_matches[1];
						transform(domain.begin(), domain.end(), domain.begin(), ::tolower); // to lowercase
						log->debug("domain: " + domain);

						std::shared_lock l(domainBlacklist.get_mutex());
						for (auto &s: domainBlacklist.get_container()) {
							if (s.rfind('.', 0) == 0 and endsWith(domain, s)) { // if it's a top level domain
								log->debug("blacklisted top-level-domain: " + domain);
								mitigateSpam(bot, message_cache, config, event.msg,
											 fmt::format("Blacklisted top-level-domain: `{}`", domain), DAY * 27, true);
								co_return;
							} else if (s == domain) {
								log->debug("blacklisted domain: " + domain);
								mitigateSpam(bot, message_cache, config, event.msg,
											 fmt::format("Blacklisted domain: `{}`", domain), DAY * 27, true);
								co_return;
							}
						}
					}
				}

				// check if url ends with a blacklisted extension
				for (const std::string ext : config["forbidden-file-extensions"]) {
					if (endsWith(URL, ext)) {
						log->debug("forbidden extension: " + ext);
						mitigateSpam(bot, message_cache, config, event.msg,
									 fmt::format("Forbidden file extension in URL: `{}`", ext), DAY * 27, true);
						co_return;
					}
				}
			}

			/*
			 * check for bad word usage.
			 * It loops through each word in the message and checks if its starting or ending with a bad word.
			 */
			std::string message = event.msg.content;
			std::regex replacementRegex(R"([!,.\n\r\t\0])", std::regex_constants::icase);
			message = regex_replace(message, replacementRegex, "$6"); // remove special chars
			transform(message.begin(), message.end(), message.begin(), ::tolower); // to lowercase

			std::shared_lock l(forbiddenWords.get_mutex());
			for (auto &forbiddenWord : forbiddenWords.get_container()) { // iterate through all lines of the bad words config
				if (!forbiddenWord.empty()) {
					if (message.find(" " + forbiddenWord) != std::string::npos or message.find(forbiddenWord + " ") != std::string::npos or
					message.rfind(forbiddenWord, 0) == 0 or endsWith(message, forbiddenWord)) { // search the bad word in the message
						log->debug("bad word: " + forbiddenWord);
						mitigateSpam(bot, message_cache, config, event.msg,
									 fmt::format("Use of a bad word: `{}`", forbiddenWord), 660, false);
						co_return;
					}
				}
			}
		}

		// to many urls in general
		if (urlCount > 8) {
			log->debug("too many urls");
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Too many URLs", DAY * 27, true);
			co_return;
		}

		// too many mentions
		if (event.msg.mentions.size() > 6) {
			log->debug("too many mentions in one message");
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Mass ping", DAY * 14, false);
			co_return;
		}

		// check if @everyone and a link
		if (urlCount >= 1 and (event.msg.content.find("@everyone") != std::string::npos or
							   event.msg.content.find("@here") != std::string::npos)) {
			log->debug("url with @everyone-mention");
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Message contains URL and @everyone", DAY * (inviteCount >= 1 ? 27 : 14), true);
			co_return;
		}

		// check for blacklisted file extensions
		for (const dpp::attachment &attachment: event.msg.attachments) {
			for (const std::string ext : config["forbidden-file-extensions"]) {
				if (endsWith(attachment.filename, ext)) {
					log->debug("forbidden file extension: " + ext);
					mitigateSpam(bot, message_cache, config, event.msg,
								 fmt::format("Forbidden file extension: `{}`", ext), DAY * 27, true);
					co_return;
				}
			}
		}

		/*
		 * Define some counters as vectors to analyze the users message history.
		 * Goes through the message cache for this.
		 */

		std::set<dpp::snowflake> different_channel_ids; //!< IDs of all different channel in which the user has sent messages

		std::vector<dpp::message *> same_messages_in_different_channels; //!< counter for how many channels the user sent the same message in

		std::vector<dpp::message *> same_messages_in_same_channel;

		std::vector<dpp::message *> same_attachment; //!< count how many times the user has sent the same attachment or sticker

		uint32_t mention_count = 0; //!< how many mentions in all messages (including cached) from the user

		/* https://github.com/brainboxdotcc/DPP/blob/a7c9e02253707fa73f242d8c07b489d13f95e14a/src/dpp/discordclient.cpp#L514-L548 */
		/** a list of too old messages that could be removed */
		std::vector<dpp::message *> to_remove;
		std::unordered_map<dpp::snowflake, dpp::message *> &mc = message_cache.get_container();
		std::shared_lock message_cache_lock(message_cache.get_mutex());

		for (const auto &[id, msg]: mc) {

			// current utc time
			time_t t = time(nullptr);
			auto local_field = *gmtime(&t);
			auto utc = mktime(&local_field);

			// check if the message in the cache is older than 5 minutes. If so, remove it from the cache
			if (difftime(utc, msg->sent) > MINUTE * 5) {
				to_remove.push_back(msg);
			} else {
				// count in how many channels the user wrote in the last 40 seconds
				if (msg->author.id == event.msg.author.id and
					difftime(utc, msg->sent) < 40) {
					if (different_channel_ids.find(msg->channel_id) == different_channel_ids.end()) {
						different_channel_ids.insert(msg->channel_id);
					}
				}

				// count channels with the same message in the last 130 seconds
				if (msg->author.id == event.msg.author.id and
					difftime(utc, msg->sent) < 130 and
					!msg->content.empty() and
					!event.msg.content.empty() and
					msg->content == event.msg.content and
					!bool(msg->message_reference.message_id)) {
					bool already_added = false;
					for (const dpp::message *m: same_messages_in_different_channels) {
						if (m->channel_id == msg->channel_id) {
							already_added = true;
							break;
						}
					}
					if (!already_added) {
						same_messages_in_different_channels.push_back(msg);
					}
				}

				// same attachment or sticker sent somewhere from the user in the last 50 seconds
				if (msg->author.id == event.msg.author.id and
					difftime(utc, msg->sent) < 50) {
					for (const dpp::attachment &a: msg->attachments) {
						for (const dpp::attachment &event_a: event.msg.attachments) {
							if (a.size == event_a.size) {
								same_attachment.push_back(msg);
								goto skip_same_attachment_check;
							}
						}
					}
					for (const dpp::sticker &sticker: msg->stickers) {
						for (const dpp::sticker &event_s: event.msg.stickers) {
							if (sticker.id == event_s.id) {
								same_attachment.push_back(msg);
								goto skip_same_attachment_check;
							}
						}
					}
				}
				skip_same_attachment_check:

				// count same messages in the same channel from the last 40 seconds
				if (msg->author.id == event.msg.author.id and
					difftime(utc, msg->sent) < 40 and
					!msg->content.empty() and
					!event.msg.content.empty() and
					msg->content == event.msg.content and
					!bool(msg->message_reference.message_id) and
					msg->channel_id == event.msg.channel_id) {
					same_messages_in_same_channel.push_back(msg);
				}

				// count amount of mentions in his last messages from the last 50 seconds
				if (msg->author.id == event.msg.author.id and
					difftime(utc, msg->sent) < 50 and
					!msg->content.empty() and
					!event.msg.content.empty()) {
					mention_count += event.msg.mentions.size();
				}
			}
		}
		message_cache_lock.unlock();

		// remove too old messages from the cache
		for (dpp::message *mp: to_remove) {
			message_cache.remove(mp); // will lock the container with unique mutex
		}

		// too many repeated messages in multiple channels
		if (same_messages_in_different_channels.size() >= 3) {
			std::string channelMentions;
			for (auto &s : same_messages_in_different_channels) {
				channelMentions += fmt::format("<#{}>", s->channel_id);
			}
			mitigateSpam(bot, message_cache, config, event.msg,
						 fmt::format("Crossposted message in {} Channels:||{}||",
									 same_messages_in_different_channels.size(), channelMentions), DAY * (urlCount >= 1 ? 27 : 14), true); // maximum mute duration if contains url
			co_return;
		}

		// too many repeated messages in the same channel
		if (same_messages_in_same_channel.size() >= 4) {
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Repeated message 4 times", DAY * (urlCount >= 1 ? 27 : 8), true); // maximum mute duration if contains url
			co_return;
		}

		// same attachment or sticker sent somewhere
		if (same_attachment.size() >= 4) {
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Repeated attachment 4 times", DAY * 7, true);
			co_return;
		}

		if (mention_count > 20) {
			log->debug("too many mentions in multiple messages");
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Too many mentions through multiple messages", DAY * 27, true);
			co_return;
		}

		// count total messages in all channels
		if (different_channel_ids.size() >= 8) {
			std::string channelMentions;
			for (auto &id : different_channel_ids) {
				channelMentions += "<#" + std::to_string(id) + ">";
			}
			mitigateSpam(bot, message_cache, config, event.msg,
						 fmt::format("Sent messages in a shorter time in {} Channels:||{}||",
									 different_channel_ids.size(), channelMentions), DAY * (urlCount >= 1 ? 27 : 1), true); // maximum mute duration if contains url
			co_return;
		}

		// too many discord-invitations in one message
		if (inviteCount > 4 or inviteCodes.size() >= 2) {
			log->debug("too many invites");
			mitigateSpam(bot, message_cache, config, event.msg,
						 "Too many invitations", DAY * 27, true);
			co_return;
		} else {
			// check for invitation to another server
			for (const std::string &inviteCode: inviteCodes) {
				log->debug("invite code: " + inviteCode);
				auto confirmation = co_await bot.co_invite_get(inviteCode);
				if (!confirmation.is_error()) {
					auto invite = confirmation.get<dpp::invite>();
					if (invite.guild_id == event.msg.guild_id) {
						continue; // do nothing when its from the same guild
					}
					bot.log(dpp::ll_debug, "invite detected: " + invite.code);
				} else {
					// unknown or invalid invite
					bot.log(dpp::ll_debug, "unknown invite detected");
				}

				std::vector<dpp::embed_field> embedFields;

				if (!confirmation.is_error()) {
					// invite details
					auto invite = std::get<dpp::invite>(confirmation.value);
					dpp::embed_field inviteField;
					inviteField.is_inline = false;
					inviteField.name = "Invitation details";
					std::string s = "\nCode: _" + invite.code + "_";
					if (invite.inviter_id) {
						s = "Created by: <@" + std::to_string(invite.inviter_id) + ">";
					}
					if (invite.expires_at) {
						s += "\nExpires: " + dpp::utility::timestamp(invite.expires_at, dpp::utility::tf_short_datetime);
					}
					s += "\nGuild ID: " + std::to_string(invite.guild_id);
					inviteField.value = s;
					embedFields.push_back(inviteField);

					// get the guild object from the raw json
					nlohmann::json j;
					try {
						j = nlohmann::json::parse(confirmation.http_info.body);
					} catch (nlohmann::json::exception&) {}
					if (j.contains("guild")) {
						dpp::embed_field guildField;
						guildField.is_inline = false;
						guildField.name = "Guild info";
						auto guild = dpp::guild().fill_from_json(&j["guild"]);
						std::string gInfo = "Name: " + guild.name;
						if (!guild.description.empty()) {
							gInfo += "\nDescription: " + guild.description;
						}
						gInfo += "\nMembers: :green_circle: " + std::to_string(invite.approximate_presence_count) +
								 " Online • " + std::to_string(invite.approximate_member_count) + " Total members";
						guildField.value = gInfo;
						embedFields.push_back(guildField);
					}
				}

				mitigateSpam(bot, message_cache, config, event.msg,
							 "Invitation posted", DAY * 27, true, embedFields);
			}
		}
	});

	/* Start bot */
	bot.start(false);

	return 0;
}