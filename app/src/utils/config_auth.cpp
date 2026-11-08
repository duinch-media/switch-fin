#include <borealis.hpp>
#include <fstream>
#include "api/jellyfin.hpp"
#include "utils/config.hpp"
#include "utils/misc.hpp"
#include "view/danmaku_core.hpp"

bool AppConfig::checkLogin() {
    for (auto& s : this->servers) {
        if (s.id.empty() && s.urls.size() > 0) {
            try {
                std::string url = s.urls.front() + jellyfin::apiPublicInfo;
                std::string resp = HTTP::get(url, HTTP::Timeout{3000});
                jellyfin::PublicSystemInfo info = nlohmann::json::parse(resp);
                s.id = info.Id;
                s.name = info.ServerName;
            } catch (const std::exception& ex) {
                brls::Logger::warning("AppConfig {} checkServer: {}", s.urls.front(), ex.what());
                return false;
            }
        }
    }

    auto is_user = [this](const AppUser& u) { return u.id == this->user_id; };
    auto userIter = std::find_if(this->users.begin(), this->users.end(), is_user);
    if (userIter == this->users.end()) return false;

    auto is_server = [&userIter](const AppServer& s) { return s.id == userIter->server_id; };
    auto it = std::find_if(this->servers.begin(), this->servers.end(), is_server);
    if (it == this->servers.end()) return false;

    this->server_url = it->urls.front();
    HTTP::Header header = {this->getAuth(userIter->access_token)};
    std::string uri = fmt::format("{}/Users/{}", this->server_url, this->user_id);
    try {
        std::string resp = HTTP::get(uri, header, HTTP::Timeout{});
        jellyfin::UserInfo info = nlohmann::json::parse(resp);
        userIter->is_admin = info.Policy.IsAdministrator;
        userIter->config = std::move(info.Configuration);
        return true;
    } catch (const std::exception& ex) {
        brls::Logger::warning("AppConfig {} checkLogin: {}", this->server_url, ex.what());
        return false;
    }
}

const AppUser& AppConfig::currentUser() const {
    static const AppUser empty{};
    auto it = std::find_if(this->users.begin(), this->users.end(),
        [this](const AppUser& u) { return u.id == this->user_id; });
    return it != this->users.end() ? *it : empty;
}

bool AppConfig::checkDanmuku() {
    jellyfin::getJSON<jellyfin::PluginList>(
        [](const jellyfin::PluginList& plugins) {
            for (auto& p : plugins) {
                if (p.Name == "Danmu") {
                    DanmakuCore::PLUGIN_ACTIVE = true;
                    brls::Logger::info("Danmaku plugin found: {}", p.Version);
                    return;
                }
            }
            DanmakuCore::PLUGIN_ACTIVE = false;
            brls::Logger::info("Danmaku plugin not found");
        },
        [this](const std::string& err) {
            const std::string locale = brls::Application::getPlatform()->getLocale();
            bool enable = (locale == brls::LOCALE_ZH_HANS) || (locale == brls::LOCALE_ZH_HANT);
            DanmakuCore::PLUGIN_ACTIVE = this->getItem(DANMAKU, enable);
            brls::Logger::warning("checkDanmuku {} fallback ({})", err, DanmakuCore::PLUGIN_ACTIVE);
        },
        jellyfin::apiPlugins);
    return false;
}

bool AppConfig::addServer(const AppServer& s) {
    if (s.urls.size() > 0) {
        this->server_url = s.urls.front();
    }

    for (auto& o : this->servers) {
        if (s.id == o.id) {
            if (!s.name.empty()) o.name = s.name;
            for (auto it = o.urls.begin(); it != o.urls.end(); ++it) {
                if (it->compare(this->server_url) == 0) {
                    it = o.urls.erase(it);
                    break;
                }
            }
            o.urls.insert(o.urls.begin(), this->server_url);
            this->save();
            return true;
        }
    }
    this->servers.push_back(s);
    this->save();
    return false;
}

void AppConfig::addUser(const AppUser& u, const std::string& url) {
    auto is_user = [u](const AppUser& o) { return o.id == u.id; };
    auto it = std::find_if(this->users.begin(), this->users.end(), is_user);
    if (it != this->users.end()) {
        it->name = u.name;
        it->access_token = u.access_token;
        it->server_id = u.server_id;
        it->is_admin = u.is_admin;
        it->config = std::move(u.config);
    } else {
        this->users.push_back(u);
    }
    this->server_url = url;
    this->user_id = u.id;
    this->save();
}

bool AppConfig::removeServer(const std::string& id) {
    for (auto it = this->servers.begin(); it != this->servers.end(); ++it) {
        if (it->id == id) {
            this->servers.erase(it);
            this->save();
            return this->servers.empty();
        }
    }
    return false;
}

bool AppConfig::removeUser(const std::string& id) {
    for (auto it = this->users.begin(); it != this->users.end(); ++it) {
        if (it->id == id) {
            this->users.erase(it);
            this->save();
            return true;
        }
    }
    return false;
}

void AppConfig::addRemote(const AppRemote& r) {
    this->remotes.push_back(r);
    this->save();
}

void AppConfig::updateRemote(size_t index, const AppRemote& r) {
    if (index >= this->remotes.size()) return;
    this->remotes[index] = r;
    this->save();
}

void AppConfig::removeRemote(size_t index) {
    if (index >= this->remotes.size()) return;
    this->remotes.erase(this->remotes.begin() + index);
    this->save();
}

std::string AppConfig::getAuth(const std::string& token) {
    if (this->device_name.empty()) this->device_name = AppVersion::getDeviceName();

    if (token.empty())
        return fmt::format("Authorization: MediaBrowser Client=\"{}\", Device=\"{}\", DeviceId=\"{}\", Version=\"{}\"",
            AppVersion::getPackageName(), this->device_name, this->device, AppVersion::getVersion());
    else
        return fmt::format(
            "Authorization: MediaBrowser Client=\"{}\", Device=\"{}\", DeviceId=\"{}\", Version=\"{}\", "
            "Token=\"{}\"",
            AppVersion::getPackageName(), this->device_name, this->device, AppVersion::getVersion(), token);
}

const std::vector<AppUser> AppConfig::getUsers(const std::string& id) const {
    std::vector<AppUser> users;
    for (auto& u : this->users) {
        if (u.server_id == id) {
            users.push_back(u);
        }
    }
    return users;
}

void AppConfig::save() {
    try {
        std::string dir = this->configDir();
        fs::create_directories(dir);
#if !defined(USE_BOOST_FILESYSTEM) || defined(_WIN32)
        std::ofstream f(fs::u8path(dir + "/config.json"));
#else
        std::ofstream f(dir + "/config.json");
#endif
        if (f.is_open()) {
            nlohmann::json j(*this);
            f << j.dump(2);
            f.close();
        }
    } catch (const std::exception& ex) {
        brls::Logger::warning("AppConfig save: {}", ex.what());
    }
}
