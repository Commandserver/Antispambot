#include <dpp/dpp.h>
#include <thread>

#define MAX_FILE_SIZE 8000000

dpp::permission permissions_massban() {
	return dpp::p_ban_members;
}

dpp::slashcommand definition_massban() {
	return dpp::slashcommand()
			.set_name("massban")
			.set_description("Mass ban")
			.set_default_permissions(permissions_massban())
			.add_option(dpp::command_option(dpp::co_user, "first", "The user who joined first", true))
			.add_option(dpp::command_option(dpp::co_user, "last", "The user who joined last", true));
}

void handle_massban(dpp::cluster& bot, const dpp::slashcommand_t& event) {

	auto guild = dpp::find_guild(event.command.guild_id);
	if (guild == nullptr) {
		bot.log(dpp::ll_error, "Guild not found");
		event.reply(dpp::message("Oopsie Woopsie. Uwu we made a fucky wucky!").set_flags(dpp::m_ephemeral));
		return;
	}

	// check permission
	if (!guild->base_permissions(event.command.member).has(permissions_massban())) {
		event.reply(dpp::message("You don't have enough permissions").set_flags(dpp::m_ephemeral));
		return;
	}

	// get the first and last member parameter from slash command
	dpp::guild_member* first;
	dpp::guild_member* last;
	{
		const auto f = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("first")));
		if (f == guild->members.end()) {
			event.reply(dpp::message("Member in `first` not found").set_flags(dpp::m_ephemeral));
			return;
		}
		const auto l = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("last")));
		if (l == guild->members.end()) {
			event.reply(dpp::message("Member in `last` not found").set_flags(dpp::m_ephemeral));
			return;
		}
		first = &f->second;
		last = &l->second;
	}

	if (first->user_id == last->user_id) {
		event.reply(dpp::message("The first and the last user must be different").set_flags(dpp::m_ephemeral));
		return;
	}

	if (first->joined_at > last->joined_at) {
		event.reply(dpp::message("The first user must be joined before the last").set_flags(dpp::m_ephemeral));
		return;
	}

	std::set<dpp::snowflake> users_to_ban = {first->user_id, last->user_id};

	std::string file = "| User ID\t\t| Account creation\t| Joined\t\t| Username\n";

	// find the users to ban and create the preview file
	for (const auto& m : guild->members) {
		if (m.second.joined_at >= first->joined_at && m.second.joined_at <= last->joined_at) {
			users_to_ban.insert(m.second.user_id);

			// add member to the file which is then send with the reply
			auto creation = static_cast<time_t>(m.first.get_creation_time());
			file += std::to_string(m.first) + "\t" + formatTime(creation) + "\t" + formatTime(m.second.joined_at);
			auto user = dpp::find_user(m.first);
			if (user) {
				file += "\t" + user->username;
			}
			file += "\n";
		}
	}
	// truncate the file to 8 MB to ensure the file can be uploaded to discord
	file = dpp::utility::utf8substr(file, 0, MAX_FILE_SIZE);


	auto confirm_component = dpp::component()
			.set_label("Massban")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_danger);

	ButtonHandler::bind(confirm_component, [&bot, users_to_ban, sourceId = event.command.usr.id](const dpp::button_click_t &event) {
		// check if the user is the invoking user of the slash command
		if (sourceId != event.command.usr.id) {
			event.reply(dpp::message("You can't trigger that button").set_flags(dpp::m_ephemeral));
			return false;
		}

		// check if the user has ban permissions
		try {
			if (!event.command.get_resolved_permission(event.command.usr.id).has(dpp::p_ban_members)) {
				event.reply(dpp::message("You don't have permissions to ban members").set_flags(dpp::m_ephemeral));
				return false;
			}
		} catch (dpp::logic_exception &e) {
			event.reply(dpp::message("Couldn't resolve your permissions").set_flags(dpp::m_ephemeral));
			return false;
		}

		event.reply(dpp::message("Mass ban started! Please wait..."));

		std::thread t([&bot, users_to_ban, sourceId, guild_id = event.command.guild_id, channel_id = event.command.channel_id](){
			bot.log(dpp::ll_info, "mass ban startet in new thread with " + std::to_string(users_to_ban.size()) + " users");

			std::set<dpp::snowflake> success;
			std::set<dpp::snowflake> failures;
			std::string banReason = "mass ban by " + std::to_string(sourceId) + " at " + formatTime(time(nullptr));

			for (const auto& id : users_to_ban) {
				try {
					bot.set_audit_reason(banReason).guild_ban_add_sync(guild_id, id);
				} catch (dpp::rest_exception &exception) {
					bot.log(dpp::ll_error, "couldn't mass ban user id " + std::to_string(id));
					failures.insert(id);
					continue;
				}
				bot.log(dpp::ll_info, "mass banned user id " + std::to_string(id));
				success.insert(id);
			}

			bot.log(dpp::ll_info, "mass ban finished");
			bot.log(dpp::ll_info, fmt::format("failed to mass ban {} users", failures.size()));
			bot.log(dpp::ll_info, fmt::format("success mass banned {} users", success.size()));

			// create discord response
			dpp::embed embed;
			embed.set_title("Mass ban finished!");
			embed.add_field("Successfully banned", fmt::format("**{}** / {}", success.size(), users_to_ban.size()),true);
			embed.add_field("Failures", fmt::format("**{}** / {}", failures.size(), users_to_ban.size()), true);
			if (success.size() == users_to_ban.size()) {
				embed.set_description(":white_check_mark: looks good!");
			}
			dpp::message m;
			m.set_channel_id(channel_id);
			m.add_embed(embed);
			if (!success.empty()) {
				std::string users;
				for (const auto &id: success) {
					users += std::to_string(id) + "\n";
				}
				// truncate the users-file to 8 MB to ensure the file can be uploaded to discord
				users = dpp::utility::utf8substr(users, 0, MAX_FILE_SIZE);
				m.add_file("successfully-banned-users.txt", users);
			}
			if (!failures.empty()) {
				std::string users;
				for (const auto &id: failures) {
					users += std::to_string(id) + "\n";
				}
				// truncate the users-file to 8 MB to ensure the file can be uploaded to discord
				users = dpp::utility::utf8substr(users, 0, MAX_FILE_SIZE);
				m.add_file("failed-user-bans.txt", users);
			}
			bot.message_create_sync(m);
		});
		t.detach();
		return true;
	});

	auto cancel_confirm = dpp::component()
			.set_label("Cancel")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_secondary);

	ButtonHandler::bind(cancel_confirm, [source = event.command.usr.id](const dpp::button_click_t &event) {

		if (source != event.command.usr.id) {
			return false;
		}

		event.reply(
				dpp::interaction_response_type::ir_update_message,
				dpp::message()
						.set_flags(dpp::m_ephemeral)
						.set_content("Mass ban canceled")
		);
		return true;
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
	msg.add_file("members-to-ban.txt", file);

	event.reply(msg);
}