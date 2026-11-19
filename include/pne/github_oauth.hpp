#pragma once

// Public OAuth application configuration for PS4 Neural Engine.
// A Client ID is public application metadata; never put a Client Secret here.
namespace pne::github {
inline constexpr char kClientId[] = "0v23liEuX7IS6L79I8A6";
inline constexpr char kDeviceCodeUrl[] = "https://github.com/login/device/code";
inline constexpr char kAccessTokenUrl[] = "https://github.com/login/oauth/access_token";
inline constexpr char kVerificationUrl[] = "https://github.com/login/device";
inline constexpr char kRepository[] = "demolidorreddragons/NR";
inline constexpr char kUploadScope[] = "public_repo";
inline constexpr char kDeviceGrantType[] =
    "urn:ietf:params:oauth:grant-type:device_code";
} // namespace pne::github
