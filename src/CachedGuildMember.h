#include <dpp/dpp.h>

/**
 * dpp::cache requires that the cached objects have an id-attribute.
 * Due dpp::guild_member only has user_id, this class replaces it with id and can therefor be used in the cache.
 */
class CachedGuildMember : public dpp::managed {
public:
	dpp::guild_member gm;

	CachedGuildMember() = default;

	CachedGuildMember(const dpp::guild_member &m);
};
