#include <dpp/dpp.h>


dpp::slashcommand definition_massban() {
	return dpp::slashcommand()
			.set_name("massban")
			.set_description("Mass ban")
			.set_default_permissions(0)
			.add_option(dpp::command_option(dpp::co_user, "first", "The user who joined first", true))
			.add_option(dpp::command_option(dpp::co_user, "last", "The user who joined last", true));
}

void handle_massban(dpp::cluster& bot, const dpp::slashcommand_t& event) {

	auto guild = dpp::find_guild(event.command.guild_id);
	if (guild == nullptr) {
		bot.log(dpp::ll_error, "guild not found");
		return;
	}

	// check permission
	if (!guild->base_permissions(event.command.member).has(dpp::p_administrator)) {
		event.reply(dpp::message("You need to be administrator").set_flags(dpp::m_ephemeral));
		return;
	}

	// get the first and last member parameter from slash command
	dpp::guild_member* first;
	dpp::guild_member* last;
	{
		const auto f = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("first")));
		if (f == guild->members.end()) {
			event.reply(dpp::message("User in `first` not found").set_flags(dpp::m_ephemeral));
			return;
		}
		const auto l = guild->members.find(std::get<dpp::snowflake>(event.get_parameter("last")));
		if (l == guild->members.end()) {
			event.reply(dpp::message("User in `last` not found").set_flags(dpp::m_ephemeral));
			return;
		}
		first = &f->second;
		last = &l->second;
	}

	if (first->joined_at > last->joined_at) {
		event.reply(dpp::message("The first user must be joined before the last").set_flags(dpp::m_ephemeral));
		return;
	}

	std::set<dpp::snowflake> users_to_ban = {first->user_id, last->user_id};

	// find the users to ban
	for (const auto& m : guild->members) {
		if (m.second.joined_at >= first->joined_at && m.second.joined_at <= last->joined_at) {
			users_to_ban.insert(m.second.user_id);
		}
	}

	{
		// create response
		std::string description;

		bool interactionAlreadyAcknowledged = false;
		for (const dpp::snowflake& id : users_to_ban) {
			const dpp::managed managedObj(id);
			std::string s_to_add = "<@" + std::to_string(id) + "> " + dpp::utility::timestamp((time_t)managedObj.get_creation_time(), dpp::utility::tf_short_datetime) + "\n";

			// make sure to not exceed message limits
			if (description.size() + s_to_add.size() > 4030) {
				dpp::message m = dpp::message().add_embed(dpp::embed().set_description(description).set_title("Username | Account creation"));
				if (interactionAlreadyAcknowledged) {
					bot.message_create(m.set_guild_id(event.command.guild_id).set_channel_id(event.command.channel_id));
				} else {
					interactionAlreadyAcknowledged = true;
					event.reply(m);
				}
				description = "";
			}
			description += s_to_add;
		}

		dpp::message m = dpp::message().add_embed(dpp::embed().set_description(description).set_title("Username | Account creation"));
		if (interactionAlreadyAcknowledged) {
			bot.message_create(m.set_guild_id(event.command.guild_id).set_channel_id(event.command.channel_id));
		} else {
			interactionAlreadyAcknowledged = true;
			event.reply(m);
		}
		description = "";

	}

	{
		// send confirm buttons

		auto confirm_component = dpp::component()
				.set_label("Ban")
				.set_type(dpp::cot_button)
				.set_style(dpp::cos_danger);

		bindComponentAction(confirm_component, [&bot, users_to_ban](const dpp::button_click_t &event) {
			event.reply(
					dpp::message("Mass ban started!")
			);
			// TODO implement ban stuff
		});

		auto cancel_confirm = dpp::component()
				.set_label("Cancel")
				.set_type(dpp::cot_button)
				.set_style(dpp::cos_secondary);

		bindComponentAction(cancel_confirm, [](const dpp::button_click_t &event) {
			event.reply(
					dpp::interaction_response_type::ir_update_message,
					dpp::message()
							.set_flags(dpp::m_ephemeral)
							.set_content("Mass ban canceled")
			);
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
		msg.set_guild_id(event.command.guild_id).set_channel_id(event.command.channel_id);

		bot.message_create(msg);
	}
}