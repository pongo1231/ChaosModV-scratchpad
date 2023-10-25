#include "json.h"
#include "string_helpers.h"

#include <httpserver.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/endpoint.hpp>
#include <websocketpp/server.hpp>

#include <chrono>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

using server         = websocketpp::server<websocketpp::config::asio>;
using connection_ptr = websocketpp::endpoint<websocketpp::connection<websocketpp::config::asio>,
                                             websocketpp::config::asio>::connection_ptr;
using message_ptr    = server::message_ptr;

using namespace httpserver;

static server chaos_server;
static std::vector<connection_ptr> connections;
static std::mutex connections_mutex;

static std::string last_init_msg;

struct effect_data
{
	std::string id;
	std::string name;
	std::uint16_t cooldown = 0;
};
static std::vector<effect_data> effects;
static std::mutex effects_mutex;

static std::string token;
static std::string password;
static std::uint16_t cooldown = 0;

class chaoswebserver : public http_resource
{
  private:
	const std::shared_ptr<http_response> render_GET(const http_request &request) override
	{
		auto path = request.get_path();
		if (path.find("/..") != path.npos)
			return std::make_shared<string_response>("lol no", 404);

		auto requestor = request.get_requestor();
		if (!password.empty() && request.get_cookie("token") != token)
		{
			if (path == "/" || path == "/index.html")
				return std::make_shared<file_response>("login.html", 200, "text/html");

			return std::make_shared<string_response>("forbidden", 403);
		}

		if (path == "/fetch_effects")
		{
			nlohmann::json json;
			if (!connections.empty())
			{
				std::unique_lock lock(effects_mutex);
				for (const auto &effect : effects)
				{
					nlohmann::json object;
					object["id"]       = effect.id;
					object["name"]     = effect.name;
					object["cooldown"] = effect.cooldown;
					json.push_back(object);
				}
				lock.unlock();
			}

			return std::make_shared<string_response>(json.dump(), 200, "application/json");
		}

		path           = path == "/" ? "index.html" : path.substr(1);
		auto mime_type = "text/html";
		if (path.ends_with(".js"))
			mime_type = "text/javascript";
		else if (path.ends_with(".css"))
			mime_type = "text/css";
		return std::make_shared<file_response>(path, 200, mime_type);
	}

	const std::shared_ptr<http_response> render_POST(const http_request &request) override
	{
		auto path      = request.get_path();
		auto args      = request.get_args();

		auto requestor = request.get_requestor();
		if (!password.empty() && request.get_cookie("token") != token)
		{
			if (path == "/authorize")
			{
				if (password.empty())
					return std::make_shared<string_response>("no password set", 404);

				if (!args.contains("password"))
					return std::make_shared<string_response>("unauthorized", 401);

				if (args["password"] == password)
					return std::make_shared<string_response>(token, 200);

				return std::make_shared<string_response>("unauthorized", 401);
			}

			return std::make_shared<string_response>("forbidden", 403);
		}

		if (path == "/trigger_effect")
		{
			if (!args.contains("id"))
				return std::make_shared<string_response>("missing id", 400);

			auto effect_it = std::find_if(effects.begin(), effects.end(),
			                              [&](const auto &effect) { return effect.id == args["id"]; });
			if (effect_it == effects.end() || connections.empty())
				return std::make_shared<string_response>("invalid id", 400);

			auto &effect = *effect_it;
			if (effect.cooldown)
				return std::make_shared<string_response>("-1", 400);

			std::cout << "trigger_effect: " << args["id"] << "\n";

			std::unique_lock lock(connections_mutex);
			for (auto connection : connections)
				connection->send(args["id"]);

			effect.cooldown = cooldown;
			return std::make_shared<string_response>(std::to_string(cooldown), 200);
		}

		return std::make_shared<string_response>("wat", 404);
	}
};

void on_open(websocketpp::connection_hdl handle)
{
	std::lock_guard lock(connections_mutex);

	auto connection = chaos_server.get_con_from_hdl(handle);
	connections.push_back(connection);
}

void on_close(websocketpp::connection_hdl handle)
{
	std::lock_guard lock(connections_mutex);

	auto connection = chaos_server.get_con_from_hdl(handle);
	auto element =
	    std::find_if(connections.begin(), connections.end(),
	                 [&](auto element) { return element->get_remote_endpoint() == connection->get_remote_endpoint(); });
	if (element != connections.end())
		connections.erase(element);
}

void on_message(server *server, websocketpp::connection_hdl handle, message_ptr msg)
{
	auto msg_str = trim(msg->get_payload());

	if (msg_str.starts_with("initß"))
	{
		std::cout << "Received init msg!\n";

		if (msg_str == last_init_msg)
		{
			std::cout << "Init msg equals last init msg, skipping init!\n";
			return;
		}

		std::lock_guard lock(effects_mutex);

		effects.clear();

		if (msg_str == "initß")
			return;

		auto password_pos = msg_str.find("ß");
		if (password_pos == msg_str.npos)
			return;
		password_pos += 2;

		auto cooldown_pos = msg_str.find("ß", password_pos);
		if (cooldown_pos == msg_str.npos)
			return;
		cooldown_pos += 2;

		auto effects_pos = msg_str.find("ß", cooldown_pos);
		if (effects_pos == msg_str.npos)
			return;
		effects_pos += 2;

		auto new_password = msg_str.substr(password_pos, msg_str.find("ß", password_pos) - password_pos);
		std::cout << "Password set to \"" << new_password << "\"!\n";
		if (new_password != password)
		{
			if (!password.empty())
				std::cout << "Password changed from previous password \"" << password << "\", changing token!\n";

			token    = random_string(64);

			password = new_password;
		}

		auto cooldown_str = msg_str.substr(cooldown_pos, msg_str.find("ß", cooldown_pos) - cooldown_pos);
		try
		{
			cooldown = std::stoul(cooldown_str);
		}
		catch (const std::exception &exception)
		{
			std::cout << "Invalid cooldown passed (\"" << cooldown_str << "\")!\n";
			return;
		}
		std::cout << "Cooldown set to " << cooldown << "!\n";

		auto received_effects = split(msg_str.substr(effects_pos), "ö");
		for (const auto &effect : received_effects)
		{
			auto parts = split(effect, "ä");
			if (parts.size() != 2)
				return;

			effects.push_back({ .id = parts[0], .name = parts[1] });
		}

		std::sort(effects.begin(), effects.end(), [](const auto &a, const auto &b) { return a.name < b.name; });

		last_init_msg = msg_str;
	}
}

int main()
{
	bool initialized = false;

	std::thread thread(
	    [&]()
	    {
		    // chaos_server.set_access_channels(websocketpp::log::alevel::all);
		    // chaos_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
		    chaos_server.clear_access_channels(websocketpp::log::alevel::all);

		    chaos_server.set_reuse_addr(true);

		    chaos_server.init_asio();

		    chaos_server.set_open_handler(bind(&on_open, ::_1));
		    chaos_server.set_close_handler(bind(&on_close, ::_1));
		    chaos_server.set_message_handler(bind(&on_message, &chaos_server, ::_1, ::_2));

		    chaos_server.listen(6909);
		    chaos_server.start_accept();

		    initialized = true;
		    chaos_server.run();
	    });

	while (!initialized)
	{
		sleep(1);
	}

	std::cout << "Server initialized!\n";

	static webserver server = create_webserver(6910)
	                              .use_ssl()
	                              .https_mem_key("/etc/letsencrypt/live/gopong.dev/privkey.pem")
	                              .https_mem_cert("/etc/letsencrypt/live/gopong.dev/cert.pem");
	static chaoswebserver webserver;
	server.register_resource("/", &webserver, true);
	server.start(false);

	srand(time(NULL));
	token = random_string(64);

	std::cout << "Web Server initialized!\n";

	std::thread cooldown_handler_thread(
	    [&]()
	    {
		    while (true)
		    {
			    sleep(1);

			    std::lock_guard lock(effects_mutex);
			    for (auto &effect : effects)
			    {
				    if (effect.cooldown > 0)
					    effect.cooldown--;
			    }
		    }
	    });

	std::thread ping_thread(
	    [&]()
	    {
		    while (true)
		    {
			    sleep(2);

			    std::lock_guard lock(connections_mutex);
			    for (auto connection : connections)
				    connection->send("ping");
		    }
	    });

	std::string input;
	while (true)
	{
		std::cin >> input;

		std::lock_guard lock(connections_mutex);
		for (auto connection : connections)
			connection->send(input);
		std::cout << "Sent " << input << "!\n";
	}

	return 0;
}