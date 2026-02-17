#include "../include/spotify_api.h"
#include "../include/config.h"
#include <string>

SpotifyAuthAPI::TokenResponse SpotifyAuthAPI::exchange_auth_code() {
	if (verifier.empty()) {
		throw SequenceError("verifier not initialised");
	}
	if (auth_code.empty()) {
		throw SequenceError("auth_code not initialised");
	}

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

BaseAuthAPI::AccessTokenResponse SpotifyAuthAPI::refresh_access_tkn(const std::string &refresh_tkn) {
	std::vector<std::pair<std::string, std::string>> fields = {
		{"client_id", get_setting("client_id", platform)}, 
		{"grant_type", "refresh_token"},
		{"refresh_token", refresh_tkn},
	};
	
	nlohmann::json resp;
	if(POST(/*endpoint=*/"token", fields, resp) != 200) {
		throw RequestError("invalid token response from spotify");
	}
	return AccessTokenResponse(std::move(resp));
}
