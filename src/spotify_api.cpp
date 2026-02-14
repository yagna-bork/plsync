#include "../include/spotify_api.h"
#include "../include/config.h"

SpotifyAuthAPI::TokenResponse SpotifyAuthAPI::exchange_auth_code(
	const std::string &auth_code, const std::string &verifier
) {
	std::vector<std::pair<std::string, std::string>> fields = {
		{"client_id", get_setting("client_id", platform)}, 
		{"code", auth_code},
		{"code_verifier", verifier},
		{"grant_type", "authorization_code"},
		{
			"redirect_uri", 
			get_setting("redirect_url") + ":" + get_setting("redirect_port", platform)
		}
	};

	nlohmann::json resp;
	if(POST("token", fields, resp) != 200) {
		throw RequestError("invalid token response from spotify");
	}
	validate_scopes(resp["scope"]);
	return TokenResponse(std::move(resp));
}
