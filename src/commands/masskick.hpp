#include <dpp/dpp.h>
#include <thread>

#define MAX_FILE_SIZE 8000000

dpp::permission permissions_masskick() {
	return dpp::p_kick_members;
}

dpp::slashcommand definition_masskick() {
	return dpp::slashcommand()
			.set_name("masskick")
			.set_description("Mass kick users")
			.set_default_permissions(permissions_masskick())
			.add_option(dpp::command_option(dpp::co_user, "first", "The user who joined first", true))
			.add_option(dpp::command_option(dpp::co_user, "last", "The user who joined last", true));
}

void handle_masskick(dpp::cluster& bot, const dpp::slashcommand_t& event) {

	auto guild = dpp::find_guild(event.command.guild_id);
	if (guild == nullptr) {
		bot.log(dpp::ll_error, "Guild not found");
		event.reply(dpp::message("Oopsie Woopsie. Uwu we made a fucky wucky!").set_flags(dpp::m_ephemeral));
		return;
	}

	// check permission
	if (!guild->base_permissions(event.command.member).has(permissions_masskick())) {
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

	std::set<dpp::snowflake> users_to_kick = {first->user_id, last->user_id};

	std::string file = "| User ID\t\t| Account creation\t| Joined\t\t| Username\n";

	// find the users to kick and create the preview file
	for (const auto& m : guild->members) {
		if (m.second.joined_at >= first->joined_at && m.second.joined_at <= last->joined_at) {
			users_to_kick.insert(m.second.user_id);

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
			.set_label("Masskick")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_danger);

	ButtonHandler::bind(confirm_component, [&bot, users_to_kick, sourceId = event.command.usr.id](const dpp::button_click_t &event) {
		// check if the user is the invoking user of the slash command
		if (sourceId != event.command.usr.id) {
			event.reply(dpp::message("You can't trigger that button").set_flags(dpp::m_ephemeral));
			return false;
		}

		// check if the user has kick permissions
		try {
			if (!event.command.get_resolved_permission(event.command.usr.id).has(dpp::p_kick_members)) {
				event.reply(dpp::message("You don't have permissions to kick members").set_flags(dpp::m_ephemeral));
				return false;
			}
		} catch (dpp::logic_exception &e) {
			event.reply(dpp::message("Couldn't resolve your permissions").set_flags(dpp::m_ephemeral));
			return false;
		}

		event.reply(dpp::message("Mass kick started! Please wait..."));

		std::thread t([&bot, users_to_kick, sourceId, guild_id = event.command.guild_id, channel_id = event.command.channel_id](){
			bot.log(dpp::ll_info, "mass kick startet in new thread with " + std::to_string(users_to_kick.size()) + " users");

			std::set<dpp::snowflake> success;
			std::set<dpp::snowflake> failures;
			std::string kickReason = "mass kick by " + std::to_string(sourceId) + " at " + formatTime(time(nullptr));

			for (const auto& id : users_to_kick) {
				try {
					bot.set_audit_reason(kickReason).guild_member_kick_sync(guild_id, id);
				} catch (dpp::rest_exception &exception) {
					bot.log(dpp::ll_error, "couldn't mass kick user id " + std::to_string(id));
					failures.insert(id);
					continue;
				}
				bot.log(dpp::ll_info, "mass kicked user id " + std::to_string(id));
				success.insert(id);
			}

			bot.log(dpp::ll_info, "mass kick finished");
			bot.log(dpp::ll_info, fmt::format("failed to mass kick {} users", failures.size()));
			bot.log(dpp::ll_info, fmt::format("success mass kicked {} users", success.size()));

			// create discord response
			dpp::embed embed;
			embed.set_title("Mass kick finished!");
			embed.add_field("Successfully kicked", fmt::format("**{}** / {}", success.size(), users_to_kick.size()), true);
			embed.add_field("Failures", fmt::format("**{}** / {}", failures.size(), users_to_kick.size()), true);
			if (success.size() == users_to_kick.size()) {
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
				m.add_file("successfully-kicked-users.txt", users);
			}
			if (!failures.empty()) {
				std::string users;
				for (const auto &id: failures) {
					users += std::to_string(id) + "\n";
				}
				// truncate the users-file to 8 MB to ensure the file can be uploaded to discord
				users = dpp::utility::utf8substr(users, 0, MAX_FILE_SIZE);
				m.add_file("failed-user-kicks.txt", users);
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
						.set_content("Mass kick canceled")
		);
		return true;
	});

	dpp::message msg;
	msg.set_content(fmt::format("Ready to kick **{}** members from this server?\nJoined between **{}** and **{}**",
								users_to_kick.size(),
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
	msg.add_file("members-to-kick.txt", file);

	event.reply(msg);
}