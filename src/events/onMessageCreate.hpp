#pragma once
#include "../classes/database.h"
#include "../classes/config.h"
#include <dpp/dpp.h>
#include <regex>

#include "../classes/domain_blacklist.h"

inline dpp::task<void> onMessageCreate(dpp::cluster& bot, dpp::cache<dpp::message>& message_cache, const dpp::message_create_t &event)
{
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

	// check for bypass
	auto rs = co_await db::co_query("SELECT type, mentionable_id FROM bypasses INNER JOIN guilds ON guilds.id = bypasses.guild_id WHERE guilds.guild_id = ?", {event.msg.guild_id});
	if (!rs.ok()) co_return;
	if (!rs.empty()) {
		for (auto row : rs) {
			if (row["type"] == "user") {
				if (event.msg.author.id == dpp::snowflake(row["mentionable_id"])) {
					co_return;
				}
			} else if (row["type"] == "role") {
				for (dpp::snowflake roleId: event.msg.member.get_roles()) {
					if (roleId == dpp::snowflake(row["mentionable_id"])) {
						co_return;
					}
				}
			} else if (row["type"] == "channel") {
				if (event.msg.channel_id == dpp::snowflake(row["mentionable_id"])) {
					co_return;
				}
				auto *channel = dpp::find_channel(event.msg.channel_id);
				if (channel and channel->parent_id and channel->parent_id == dpp::snowflake(row["mentionable_id"])) {
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

			bot.log(dpp::ll_debug, "URL: " + URL);

			// check if the domain or top-level-domain is blacklisted
			std::regex domain_pattern(R"(https?:\/\/.*?([^.]+\.[A-z]+)(?:\/|#|\?|:|\Z|$).*$)", std::regex_constants::icase);
			std::smatch domain_matches;
			if (regex_match(URL, domain_matches, domain_pattern)) {
				if (domain_matches.size() == 2) { // should be always 2
					std::string domain = domain_matches[1];
					domain = dpp::lowercase(domain);
					bot.log(dpp::ll_debug, "domain: " + domain);

					for (auto &s: domain_blacklist::domains) {
						if (s.rfind('.', 0) == 0 and endsWith(domain, s)) { // if it's a top level domain
							bot.log(dpp::ll_debug, "blacklisted top-level-domain: " + domain);
							mitigateSpam(bot, message_cache, event.msg,
										 fmt::format("Blacklisted top-level-domain: `{}`", domain), DAY * 27, true);
							co_return;
						} else if (s == domain) {
							bot.log(dpp::ll_debug, "blacklisted domain: " + domain);
							mitigateSpam(bot, message_cache, event.msg,
										 fmt::format("Blacklisted domain: `{}`", domain), DAY * 27, true);
							co_return;
						}
					}
				}
			}

			// check if url ends with a blacklisted extension
			for (const std::string ext : config::get("forbidden-file-extensions")) {
				if (endsWith(URL, ext)) {
					bot.log(dpp::ll_debug, "forbidden extension: " + ext);
					mitigateSpam(bot, message_cache, event.msg,
								 fmt::format("Forbidden file extension in URL: `{}`", ext), DAY * 27, true);
					co_return;
				}
			}
		}
	}

	// to many urls in general
	if (urlCount > 8) {
		bot.log(dpp::ll_debug, "too many urls");
		mitigateSpam(bot, message_cache, event.msg,
					 "Too many URLs", DAY * 27, true);
		co_return;
	}

	// too many mentions
	if (event.msg.mentions.size() > 6) {
		bot.log(dpp::ll_debug, "too many mentions in one message");
		mitigateSpam(bot, message_cache, event.msg,
					 "Mass ping", DAY * 14, false);
		co_return;
	}

	// check if @everyone and a link
	if (urlCount >= 1 and (event.msg.content.find("@everyone") != std::string::npos or
						   event.msg.content.find("@here") != std::string::npos)) {
		bot.log(dpp::ll_debug, "url with @everyone-mention");
		mitigateSpam(bot, message_cache, event.msg,
					 "Message contains URL and @everyone", DAY * (inviteCount >= 1 ? 27 : 14), true);
		co_return;
	}

	// check for blacklisted file extensions
	for (const dpp::attachment &attachment: event.msg.attachments) {
		for (const std::string ext : config::get("forbidden-file-extensions")) {
			if (endsWith(attachment.filename, ext)) {
				bot.log(dpp::ll_debug, "forbidden file extension: " + ext);
				mitigateSpam(bot, message_cache, event.msg,
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
			if (msg->guild_id != event.msg.guild_id) {
				continue;
			}

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
				!static_cast<bool>(msg->message_reference.message_id) and
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
		mitigateSpam(bot, message_cache, event.msg,
					 fmt::format("Crossposted message in {} Channels:||{}||",
								 same_messages_in_different_channels.size(), channelMentions), DAY * (urlCount >= 1 ? 27 : 14), true); // maximum mute duration if contains url
		co_return;
	}

	// too many repeated messages in the same channel
	if (same_messages_in_same_channel.size() >= 4) {
		mitigateSpam(bot, message_cache, event.msg,
					 "Repeated message 4 times", DAY * (urlCount >= 1 ? 27 : 8), true); // maximum mute duration if contains url
		co_return;
	}

	// same attachment or sticker sent somewhere
	if (same_attachment.size() >= 4) {
		mitigateSpam(bot, message_cache, event.msg,
					 "Repeated attachment 4 times", DAY * 7, true);
		co_return;
	}

	if (mention_count > 20) {
		bot.log(dpp::ll_debug, "too many mentions in multiple messages");
		mitigateSpam(bot, message_cache, event.msg,
					 "Too many mentions through multiple messages", DAY * 27, true);
		co_return;
	}

	// count total messages in all channels
	if (different_channel_ids.size() >= 8) {
		std::string channelMentions;
		for (auto &id : different_channel_ids) {
			channelMentions += "<#" + std::to_string(id) + ">";
		}
		mitigateSpam(bot, message_cache, event.msg,
					 fmt::format("Sent messages in a shorter time in {} Channels:||{}||",
								 different_channel_ids.size(), channelMentions), DAY * (urlCount >= 1 ? 27 : 1), true); // maximum mute duration if contains url
		co_return;
	}

	// too many discord-invitations in one message
	if (inviteCount > 4 or inviteCodes.size() >= 2) {
		bot.log(dpp::ll_debug, "too many invites");
		mitigateSpam(bot, message_cache, event.msg,
					 "Too many invitations", DAY * 27, true);
		co_return;
	}

	// check for invitation to another server
    for (const std::string &inviteCode: inviteCodes) {
	    bot.log(dpp::ll_debug, "invite code: " + inviteCode);
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

	    mitigateSpam(bot, message_cache, event.msg,
	                 "Invitation posted", DAY * 27, true, embedFields);
    }
    co_return;
}
