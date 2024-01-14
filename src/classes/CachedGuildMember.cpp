#include "CachedGuildMember.h"

CachedGuildMember::CachedGuildMember(const dpp::guild_member &m) {
	this->gm = m;
	this->id = m.user_id;
}