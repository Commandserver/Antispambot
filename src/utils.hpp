#pragma once
#include <dpp/dpp.h>

/**
 * Case Sensitive Implementation of endsWith()
 * It checks if the string 'mainStr' ends with given string 'toMatch'
 */
bool endsWith(const std::string &mainStr, const std::string &toMatch) {
	if (mainStr.size() >= toMatch.size() &&
		mainStr.compare(mainStr.size() - toMatch.size(), toMatch.size(), toMatch) == 0)
		return true;
	else
		return false;
}


/**
 * Format a time_t object to a human readable %Y-%m-%d %H:%M:%S time format
 */
std::string formatTime(const time_t &time) {
	char buff[20];
	strftime(buff, sizeof(buff), "%Y-%m-%d %H:%M:%S", localtime(&time));
	return buff;
}


/**
 * Make a readable time-string from seconds
 * @param seconds The amount of seconds to create a time-string for
 * @return The formatted time-string
 */
std::string stringifySeconds(uint32_t seconds) {
	uint32_t days = 0, hours = 0, minutes = 0;

	while (seconds >= 86400) {
		seconds -= 86400;
		days++;
	}
	while (seconds >= 3600) {
		seconds -= 3600;
		hours++;
	}
	while (seconds >= 60) {
		seconds -= 60;
		minutes++;
	}

	std::string result;
	if (days == 1) {
		result += std::to_string(days) + " day ";
	} else if (days > 1) {
		result += std::to_string(days) + " days ";
	}
	if (hours == 1) {
		result += std::to_string(hours) + " hour ";
	} else if (hours > 1) {
		result += std::to_string(hours) + " hours ";
	}
	if (minutes == 1) {
		result += std::to_string(minutes) + " minute ";
	} else if (minutes > 1) {
		result += std::to_string(minutes) + " minutes ";
	}
	if (seconds == 1) {
		result += std::to_string(seconds) + " second ";
	} else if (seconds > 1) {
		result += std::to_string(seconds) + " seconds ";
	}
	return result.erase(result.find_last_not_of(' ') + 1); // remove the ending space
}

/**
 * helper function to timeout the member in case of spam
 * @param bot Bot
 * @param muteDuration The amount of seconds to timeout the member
 * @param guildId
 * @param userId
 */
void muteMember(dpp::cluster &bot, const uint32_t muteDuration, const dpp::snowflake guildId, const dpp::snowflake userId) {
	try { // timeout member
		auto member = dpp::find_guild_member(guildId, userId);
		time_t muteUntil = time(nullptr) + muteDuration;
		if (member.communication_disabled_until < muteUntil) {
			bot.set_audit_reason("spam detected").guild_member_timeout(guildId, userId, muteUntil, [&bot, userId](const dpp::confirmation_callback_t &e) {
				if (e.is_error()) {
					bot.log(dpp::ll_error, "cannot timeout member " + e.http_info.body);
				} else {
					bot.log(dpp::ll_info, "timed out " + std::to_string(userId));
				}
			});
		}
	} catch (dpp::cache_exception &exception) {
		bot.log(dpp::ll_error, "couldn't find user " + std::to_string(userId) + " in cache");
	}
}

/// helper function to delete a message in case of spam
void deleteMsg(dpp::cluster &bot, const dpp::message &msg) {
	bot.message_delete(msg.id, msg.channel_id, [&bot](const dpp::confirmation_callback_t &e) {
		if (e.is_error()) {
			bot.log(dpp::ll_error, "couldn't delete message: " + e.http_info.body);
		}
	});
	bot.log(dpp::ll_debug, "added 1 message to deletion queue");
}

/// helper function to delete all cached messages of a user in case of spam
void deleteUserMessages(dpp::cluster &bot, dpp::cache<dpp::message> &message_cache, const dpp::snowflake userId) {
	std::unordered_map<dpp::snowflake, dpp::message *> &msgCache = message_cache.get_container();
	/* IMPORTANT: We must lock the container to iterate it */
	std::shared_lock l(message_cache.get_mutex());

	std::set<dpp::snowflake> channelIds; // store channel ids in which the user has written to
	for (const auto &[msgId, msg]: msgCache) {
		if (msg->author.id == userId) {
			channelIds.insert(msg->channel_id);
		}
	}
	uint32_t deletionCount = 0;
	for (const auto &channelId : channelIds) { // loop through each channel id to bulk delete the user messages whenever it's possible
		std::vector<dpp::snowflake> messageIds;
		// collect all (cached) messages from this channel sent by the user
		for (const auto &[msgId, msg]: msgCache) {
			if (msg->author.id == userId and msg->channel_id == channelId and find(messageIds.begin(), messageIds.end(), msgId) == messageIds.end()) {
				messageIds.push_back(msgId);
			}
			if (messageIds.size() >= 100) { // ensure max 100 messages to bulk delete
				break;
			}
		}
		deletionCount += messageIds.size();

		// bulk delete message ids
		if (messageIds.size() > 1) {
			bot.message_delete_bulk(messageIds, channelId, [&bot](const dpp::confirmation_callback_t &e) {
				if (e.is_error()) {
					bot.log(dpp::ll_error, "couldn't bulk delete messages: " + e.http_info.body);
				}
			});
		} else if (!messageIds.empty()) {
			bot.message_delete(messageIds[0], channelId, [&bot](const dpp::confirmation_callback_t &e) {
				if (e.is_error()) {
					bot.log(dpp::ll_error, "couldn't delete message: " + e.http_info.body);
				}
			});
		}
	}
	bot.log(dpp::ll_debug, "added " + std::to_string(deletionCount) + " messages to deletion queue");
}

/**
 * helper function in case of spam
 * @param bot bot
 * @param message_cache The message cache
 * @param config Config
 * @param msg The message which has been send
 * @param reason The reason to timeout the member
 * @param muteDuration The amount of seconds to timeout the member
 * @param clearHistoryMessages Will delete all messages from the user if set to true. If false, it will only delete the current message
 * When set to false, it deletes only this message
 */
void mitigateSpam(dpp::cluster &bot, dpp::cache<dpp::message> &message_cache, nlohmann::json &config, const dpp::message &msg, const std::string& reason, const uint32_t muteDuration, bool clearHistoryMessages) {

	muteMember(bot, muteDuration, msg.guild_id, msg.author.id);

	dpp::embed embed; // create the embed log message
	embed.set_color(0xefa226);
	embed.set_timestamp(time(nullptr));
	embed.set_description(fmt::format(":warning: Spam detected by {} ({})", msg.author.get_mention(), msg.author.format_username()));
	embed.add_field("Reason", reason, true);
	embed.add_field("Channel", fmt::format("<#{}>", msg.channel_id), true);
	if (!msg.content.empty()) {
		embed.add_field("Original message", msg.content);
	}
	embed.set_footer("ID " + std::to_string(msg.author.id), "");
	embed.add_field("Timeout duration", stringifySeconds(muteDuration));

	uint8_t i = 0; // counter to ensure only the first 3 attachments are mentioned
	for (auto &attachment : msg.attachments) {
		if (i > 3) {
			break;
		}
		i++;
		embed.add_field(
				fmt::format("Attachment {}", i),
				fmt::format("{}\n({} bytes)\n{}",
							dpp::utility::utf8substr(attachment.filename, 0, 40),
							attachment.size,
							attachment.url.size() <= 256 ? attachment.url : "_(url too long)_"
				)
		);
	}
	uint8_t j = 0; // counter to ensure only the first 2 stickers are mentioned
	for (auto &sticker : msg.stickers) {
		if (j > 2) {
			break;
		}
		j++;
		embed.add_field(
				fmt::format("Sticker {}", j),
				fmt::format("Name: {}\n_{}_\nID: {}",
							dpp::utility::utf8substr(sticker.name, 0, 30),
							dpp::utility::utf8substr(sticker.description, 0, 50),
							sticker.get_url().size() <= 256 ? sticker.get_url() : "_(url too long)_"
				)
		);
	}
	bot.execute_webhook(dpp::webhook(config["log-webhook-url"]), dpp::message().add_embed(embed));
	if (clearHistoryMessages) {
		deleteUserMessages(bot, message_cache, msg.author.id);
	} else {
		deleteMsg(bot, msg);
	}
}