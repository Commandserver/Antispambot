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

	auto confirm_component = dpp::component()
			.set_label("Kick")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_danger)
			.set_id("confirm");

	bindComponentAction(confirm_component, [](const dpp::button_click_t &event){
		event.reply(
				dpp::interaction_response_type::ir_update_message,
				dpp::message()
						.set_flags(dpp::m_ephemeral)
						.set_content("confirmed")
		);
	});

	auto cancel_confirm = dpp::component()
			.set_label("Cancel")
			.set_type(dpp::cot_button)
			.set_style(dpp::cos_secondary)
			.set_id("cancel");

	bindComponentAction(cancel_confirm, [](const dpp::button_click_t &event){
		event.reply(
				dpp::interaction_response_type::ir_update_message,
				dpp::message()
						.set_flags(dpp::m_ephemeral)
						.set_content("canceled")
		);
	});

	// TODO isn't finished yet. needs to check the permissions and actually ban lol

	dpp::message msg("Do you want to kick? Press the button below to confirm");

	msg.add_component(
			dpp::component()
					.add_component(
							confirm_component
					).add_component(
							cancel_confirm
			)
	);

	event.reply(
			msg.set_flags(dpp::m_ephemeral)
	);
}