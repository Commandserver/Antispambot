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

#define DAY 86400 //!< amount of seconds of a day
#define MINUTE 60 //!< amount of seconds of a minute

#define FAST_JOIN_THRESHOLD 60 //!< The time in seconds in which a certain number of players have to join, to be recognized as a raid. You can adjust this if you want
#define FAST_JOIN_MIN_MEMBER_COUNT 7 //!< When more than x members joined in the past (FAST_JOIN_THRESHOLD) seconds. You can adjust this number depending on your guild size

#include "classes/config.h"
#include "classes/domain_blacklist.h"
#include "classes/ButtonHandler.hpp"

#include "utils.hpp"
#include "classes/database.h"
#include "classes/ConfigSet.h"
#include "classes/CachedGuildMember.h"

#include "commands/info.hpp"
#include "commands/config.hpp"
#include "commands/massban.hpp"
#include "commands/masskick.hpp"

#include "events/onMessageCreate.hpp"


int main() {
	config::init("../config.json");
	domain_blacklist::init("../domain-blacklist.txt");
	dpp::cluster bot(config::get("token"), dpp::i_guilds | dpp::i_guild_messages | dpp::i_guild_members | dpp::i_message_content);

	bot.on_log(dpp::utility::cout_logger());

	bot.on_ready([&bot](const dpp::ready_t &event) {
		bot.log(dpp::ll_info, "Logged in as " + bot.me.username + "! Using " + dpp::utility::version());

		if (dpp::run_once<struct register_bot_commands>()) {
			std::vector<dpp::slashcommand> commands = {
					definition_info(),
					definition_config(),
					definition_massban(),
					definition_masskick(),
			};

			bot.guild_bulk_command_create(commands, config::get("guild-id").get<std::uint64_t>());
		}
	});

	bot.on_slashcommand([&bot](const dpp::slashcommand_t &event) -> dpp::task<void> {
		if (event.command.get_command_name() == "config") {
			handle_config(event);
		} else if (event.command.get_command_name() == "info") {
			handle_info(bot, event);
			bot.shutdown();
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
	const uint16_t thread_rate_limit = config::get("thread-rate-limit").get<std::uint16_t>();
	if (thread_rate_limit > 0) {
		bot.on_thread_create([&bot, &thread_rate_limit](const dpp::thread_create_t &event) {
			auto t = event.created;
			t.rate_limit_per_user = thread_rate_limit;
			bot.channel_edit(t);
		});
	}

	/// Holds all incoming messages for some time for the spam detection
	dpp::cache<dpp::message> message_cache;

	bot.on_message_create([&bot, &message_cache](const dpp::message_create_t &event) -> dpp::task<void> {
		co_await onMessageCreate(bot, message_cache, event);
	});

	/// Caches all joining members for raid detection
	dpp::cache<CachedGuildMember> guild_member_cache;

	/// usually empty. filled with members that are too fast joined
	dpp::cache<CachedGuildMember> fast_joined_members;

	time_t first_join = 0;

	bot.start_timer([&guild_member_cache, &first_join, &fast_joined_members, &bot](unsigned int timerId){

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
			bot.log(dpp::ll_info, "raid detected detected, with " + std::to_string(fast_joined_members.count()) + " members");

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
			msg.channel_id = config::get("log-channel-id").get<std::uint64_t>();
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
		if (event.adding_guild->id != config::get("guild-id").get<std::uint64_t>()) return;

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

	db::init(bot);
	bot.start(false);

	return 0;
}