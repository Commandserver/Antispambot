#include <dpp/dpp.h>

dpp::permission permissions_massban() {
	return dpp::p_administrator;
}

dpp::slashcommand definition_massban() {
	return dpp::slashcommand()
			.set_name("massban")
			.set_description("Mass ban users")
			.set_default_permissions(permissions_massban())
			.add_option(dpp::command_option(dpp::co_user, "first", "The user who joined first", true))
			.add_option(dpp::command_option(dpp::co_user, "last", "The user who joined last", true));
}

inline dpp::task<bool> triggerMassBan(dpp::cluster& bot, std::set<dpp::snowflake> users, dpp::snowflake slashCommandAuthorId, const dpp::button_click_t &event) {
	// check if the user is the invoking user of the slash command
	if (slashCommandAuthorId != event.command.usr.id) {
		event.reply(dpp::message("You can't trigger that button").set_flags(dpp::m_ephemeral));
		co_return false;
	}

	event.reply(dpp::message("Mass ban started! Please wait..."));

	bot.log(dpp::ll_info, "mass ban startet in new thread with " + std::to_string(users.size()) + " users");

	std::set<dpp::snowflake> success;
	std::set<dpp::snowflake> failures;
	std::string banReason = "mass ban by " + std::to_string(slashCommandAuthorId) + " at " + formatTime(time(nullptr));

	for (const auto& id : users) {
		auto confirmation = co_await bot.set_audit_reason(banReason).co_guild_ban_add(event.command.guild_id, id);
		if (confirmation.is_error()) {
			bot.log(dpp::ll_error, "couldn't mass ban user id " + std::to_string(id) + ": " + confirmation.get_error().message);
			failures.insert(id);
		} else {
			bot.log(dpp::ll_info, "mass banned user id " + std::to_string(id));
			success.insert(id);
		}
	}

	bot.log(dpp::ll_info, "mass ban finished");
	bot.log(dpp::ll_info, fmt::format("failed to mass ban {} users", failures.size()));
	bot.log(dpp::ll_info, fmt::format("success mass banned {} users", success.size()));

	// create discord response
	dpp::embed embed;
	embed.set_title("Mass ban finished!");
	embed.add_field("Successfully banned", fmt::format("**{}** / {}", success.size(), users.size()),true);
	embed.add_field("Failures", fmt::format("**{}** / {}", failures.size(), users.size()), true);
	if (success.size() == users.size()) {
		embed.set_description(":white_check_mark: looks good!");
	}
	dpp::message m;
	m.set_channel_id(event.command.channel_id);
	m.add_embed(embed);
	if (!success.empty()) {
		std::string s;
		for (const auto &id: success) {
			s += std::to_string(id) + "\n";
		}
		// truncate the users-file to 8 MB to ensure the file can be uploaded to discord
		s = dpp::utility::utf8substr(s, 0, MAX_DISCORD_FILE_SIZE);
		m.add_file("successfully-banned-users.txt", s);
	}
	if (!failures.empty()) {
		std::string s;
		for (const auto &id: failures) {
			s += std::to_string(id) + "\n";
		}
		// truncate the users-file to 8 MB to ensure the file can be uploaded to discord
		s = dpp::utility::utf8substr(s, 0, MAX_DISCORD_FILE_SIZE);
		m.add_file("failed-user-bans.txt", s);
	}
	co_await event.co_edit_response(m);
	co_return true;
}

dpp::task<void> handle_massban(dpp::cluster& bot, const dpp::slashcommand_t& event) {

	auto guild = dpp::find_guild(event.command.guild_id);
	if (guild == nullptr) {
		bot.log(dpp::ll_error, "Guild not found: " + event.command.guild_id);
		event.reply(dpp::message("Internal Error occurred!").set_flags(dpp::m_ephemeral));
		co_return;
	}

	// get the first and last member parameter from slash command
	dpp::guild_member* first;
	dpp::guild_member* last;
	{
		const auto f = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("first")));
		if (f == guild->members.end()) {
			event.reply(dpp::message("Member in `first` not found").set_flags(dpp::m_ephemeral));
			co_return;
		}
		const auto l = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("last")));
		if (l == guild->members.end()) {
			event.reply(dpp::message("Member in `last` not found").set_flags(dpp::m_ephemeral));
			co_return;
		}
		first = &f->second;
		last = &l->second;
	}

	if (first->user_id == last->user_id) {
		event.reply(dpp::message("The first and the last user must be different").set_flags(dpp::m_ephemeral));
		co_return;
	}

	if (first->joined_at > last->joined_at) {
		event.reply(dpp::message("The first user must be joined before the last").set_flags(dpp::m_ephemeral));
		co_return;
	}

	std::set<dpp::snowflake> users_to_ban = getUsersJoinedBetweenTime(*guild, first->joined_at, last->joined_at);

	std::string file = makeMassActionUserCSV(*guild, users_to_ban);

	auto confirm_component = dpp::component()
			.set_label("Massban")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_danger);

	ButtonHandler::bind(confirm_component, [&bot, users_to_ban, sourceId = event.command.usr.id](const dpp::button_click_t &event) -> dpp::task<bool> {
		co_return co_await triggerMassBan(bot, users_to_ban, sourceId, event);
	});

	auto cancel_confirm = dpp::component()
			.set_label("Cancel")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_secondary);

	ButtonHandler::bind(cancel_confirm, [authorId = event.command.usr.id](const dpp::button_click_t &event) -> dpp::task<bool> {
		if (authorId != event.command.usr.id) {
			co_return false;
		}

		event.reply(
				dpp::interaction_response_type::ir_update_message,
				dpp::message()
						.set_flags(dpp::m_ephemeral)
						.set_content("Mass ban canceled")
		);
		co_return true;
	});

	dpp::message msg;
	msg.set_content(fmt::format("Ready to ban **{}** members from this server?\nJoined between **{}** and **{}**",
								users_to_ban.size(),
								dpp::utility::timestamp(first->joined_at, dpp::utility::tf_short_datetime),
								dpp::utility::timestamp(last->joined_at, dpp::utility::tf_short_datetime))
	);
	msg.add_component(
			dpp::component()
					.add_component(
							confirm_component
					).add_component(
					cancel_confirm
			)
	);
	msg.set_channel_id(event.command.channel_id);
	msg.add_file("members-to-ban.csv", file);

	event.reply(msg);
}