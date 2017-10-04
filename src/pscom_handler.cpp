#include "pscom_handler.hpp"

#include <stdexcept>
#include <regex>

#include <libssh/libssh.h>
#include <fast-lib/log.hpp>
#include <fast-lib/optional.hpp>

#define EXEC_BUF_SIZE 64

std::string Pscom_handler::request_topic_template = "fast/pscom/<vm_name>/any_proc/request";
std::string Pscom_handler::response_topic_template = "fast/pscom/<vm_name>/+/response";
int Pscom_handler::qos = 0;

FASTLIB_LOG_INIT(pscom_handler_log, "Pscom_handler")
FASTLIB_LOG_SET_LEVEL_GLOBAL(pscom_handler_log, trace);

std::array<std::string, 2> test_commands = {
	"/opt/parastation/bin/psiadmin -d -c 'l p -1' | perl -n -a -e 'print if /^ / and $F[5] >= 0' | wc -l", 	// psid processes
	"/usr/bin/pgrep -P `/usr/bin/pgrep hydra_pmi_proxy` |wc -l" 						// hydra processes
};
unsigned int pscom_process_auto_detection(const std::string &vm_name) {
	try {
		ssh_session session;
		ssh_channel channel;
		if ((session = ssh_new()) == NULL) {
			throw std::runtime_error(
			    "Failed to create SSH sesseion");
		}
		ssh_options_set(session, SSH_OPTIONS_HOST, vm_name.c_str());
		FASTLIB_LOG(pscom_handler_log, trace) << "Connect to " << vm_name << " and determine pscom procs.";
		ssh_connect(session);
		ssh_userauth_publickey_auto(session, NULL, NULL);

		unsigned int messages_expected = 0;
		for (const std::string &cmd : test_commands) {
			if ((channel = ssh_channel_new(session)) == NULL) {
				throw std::runtime_error(
				    "Failed to create SSH channel");
			}
			if (ssh_channel_open_session(channel) != SSH_OK) {
				throw std::runtime_error(
				    "Failed to open SSH session to: " +
				    vm_name);
				//	throw
			}
			if (ssh_channel_request_exec(channel, cmd.c_str()) !=
			    SSH_OK) {
				throw std::runtime_error(
				    "Failed to execute command via SSH: " +
				    cmd);
			}
			char res[EXEC_BUF_SIZE];
			for (unsigned int bytes_read = -1, total_bytes = 0;
			     (bytes_read != 0) &&
			     (total_bytes <= EXEC_BUF_SIZE);
			     total_bytes += bytes_read) {
				bytes_read = ssh_channel_read_timeout(
				    channel,
				    static_cast<char *>(res + total_bytes),
				    sizeof(res) - total_bytes, false, -1);
			}
			messages_expected = std::stoi(res);
			ssh_channel_send_eof(channel);
			ssh_channel_close(channel);
			ssh_channel_free(channel);
			if (messages_expected > 0) {
				break;
			}
		}
		FASTLIB_LOG(pscom_handler_log, debug) << "Determined " << messages_expected << " running pscom processes.";
		ssh_disconnect(session);
		ssh_free(session);
		return messages_expected;
	} catch (const std::exception &e) {
		throw std::runtime_error(
		    "Exception while connecting with SSH: " +
		    std::string(e.what()));
	}
}

Pscom_handler::Pscom_handler(const fast::msg::migfra::Migrate &task,
			     std::shared_ptr<fast::Communicator> comm,
			     fast::msg::migfra::Time_measurement &time_measurement,
			     bool second_domain_swap) :
	vm_name(second_domain_swap ? task.swap_with.get().vm_name : task.vm_name),
	messages_expected_auto(false),
	messages_expected(0),
	answers(0),
	time_measurement(time_measurement),
	request_topic(std::regex_replace(request_topic_template, std::regex(R"((<vm_name>))"), vm_name)),
	response_topic(std::regex_replace(response_topic_template, std::regex(R"((<vm_name>))"), vm_name))
{
	// Autodetect pscom process count if pscom_hook_procs is auto (sets messages_expected)
	[&](const fast::Optional<std::string> &pscom_hook_procs) {
		if (pscom_hook_procs.is_valid()) {
			if (pscom_hook_procs.get() == "auto") {
				messages_expected = pscom_process_auto_detection(vm_name);
			} else {
				try {
					messages_expected = std::stoul(pscom_hook_procs.get());
				} catch (const std::exception &e) {
					throw std::runtime_error("pscom-hook-procs is malformed: " + std::string(e.what()));
				}
			}
		}
	}(second_domain_swap ? task.swap_with.get().pscom_hook_procs : task.pscom_hook_procs);


	if (messages_expected > 0) {
		if (!(this->comm = std::dynamic_pointer_cast<fast::MQTT_communicator>(comm)))
			throw std::runtime_error("Suspending pscom procs is not available without MQTT_communicator.");
		// add subscription to response topic
		this->comm->add_subscription(response_topic, 0);
		// request shutdown
		suspend();
	}
}

Pscom_handler::~Pscom_handler() noexcept(false)
{
	if (messages_expected > 0) {
		try {
			// request resume
			resume();
			// remove subscription
			comm->remove_subscription(response_topic);
		} catch (...) {
			try { // try to remove subscription but do not throw
				comm->remove_subscription(response_topic);
			} catch (...) {}
			// If not during stack unwinding rethrow exception
			if (!std::uncaught_exception())
				throw;
		}
	}
}

void Pscom_handler::set_request_topic_template(std::string request)
{
	Pscom_handler::request_topic_template = std::move(request);
}

void Pscom_handler::set_response_topic_template(std::string response)
{
	Pscom_handler::response_topic_template = std::move(response);
}

void Pscom_handler::set_qos(int qos)
{
	Pscom_handler::qos = qos;
}

void Pscom_handler::suspend()
{
	if (messages_expected > 0) {
		time_measurement.tick("pscom-suspend-" + vm_name);
		std::string msg = "suspend";
		// publish suspend request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
		time_measurement.tock("pscom-suspend-" + vm_name);
	}
}

void Pscom_handler::resume()
{
	// only try to resume if pscom is suspended
	if (answers == messages_expected && messages_expected > 0) {
		time_measurement.tick("pscom-resume-" + vm_name);
		std::string msg = "resume";
		// publish resume request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
		// reset answers counter
		answers = 0;
		time_measurement.tock("pscom-resume-" + vm_name);
	}
}
