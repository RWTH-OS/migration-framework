#include "pscom_handler.hpp"

#include <stdexcept>
#include <regex>

#include <libssh/libsshpp.hpp>
#include <fast-lib/log.hpp>

#define EXEC_BUF_SIZE 64

std::string Pscom_handler::request_topic_template = "fast/pscom/<vm_name>/any_proc/request";
std::string Pscom_handler::response_topic_template = "fast/pscom/<vm_name>/+/response";
int Pscom_handler::qos = 0;

FASTLIB_LOG_INIT(pscom_handler_log, "Pscom_handler")
FASTLIB_LOG_SET_LEVEL_GLOBAL(pscom_handler_log, trace);

Pscom_handler::Pscom_handler(const fast::msg::migfra::Migrate &task,
			     std::shared_ptr<fast::Communicator> comm,
			     fast::msg::migfra::Time_measurement &time_measurement) :
	vm_name(task.vm_name),
	messages_expected(task.pscom_hook_procs.is_valid() ? task.pscom_hook_procs.get() : 0),
	answers(0),
	time_measurement(time_measurement),
	request_topic(std::regex_replace(request_topic_template, std::regex(R"((<vm_name>))"), vm_name)),
	response_topic(std::regex_replace(response_topic_template, std::regex(R"((<vm_name>))"), vm_name))
{
	// Autodetect pscom process count if messages_expected equals 0
	if (messages_expected == 0) {
		ssh::Session session;
		ssh::Channel *channel = new ssh::Channel(session);
		session.setOption(SSH_OPTIONS_HOST, vm_name.c_str());
		try {
			FASTLIB_LOG(pscom_handler_log, trace) << "Connect to " << vm_name << " with and determine pscom procs.";
			session.connect();
			session.userauthPublickeyAuto();
			channel->openSession();
			channel->requestExec("/opt/parastation/bin/psiadmin -d -c 'l p -1' | perl -n -a -e 'print if /^ / and $F[5] >= 0' | wc -l");
			unsigned int bytes_read=-1, total_bytes;
			char res[EXEC_BUF_SIZE];
			for (total_bytes=0; (bytes_read != 0) && (total_bytes <= EXEC_BUF_SIZE); total_bytes+=bytes_read) {
				bytes_read = channel->read(static_cast<char*>(res+total_bytes), sizeof(res)-total_bytes);
			}
			messages_expected = std::stoi(res);
			FASTLIB_LOG(pscom_handler_log, debug) << "Determined " << messages_expected << " running pscom processes.";
			channel->close();
			delete channel;
			session.disconnect();
		} catch (ssh::SshException &e) {
			FASTLIB_LOG(pscom_handler_log, debug) << "Exception while connecting with SSH: " << e.getError();
		}
	}

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
		time_measurement.tick("pscom-suspend");
		std::string msg = "*";
		// publish suspend request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
		time_measurement.tock("pscom-resume");
	}
}

void Pscom_handler::resume()
{
	// only try to resume if pscom is suspended
	if (answers == messages_expected && messages_expected > 0) {
		time_measurement.tick("pscom-resume");
		std::string msg = "*";
		// publish resume request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
		// reset answers counter
		answers = 0;
		time_measurement.tock("pscom-resume");
	}
}
