#include "hooks.hpp"

#include <stdexcept>

Suspend_pscom::Suspend_pscom(const std::string &vm_name,
			     unsigned int messages_expected,
			     std::shared_ptr<fast::Communicator> comm) :
	vm_name(vm_name),
	messages_expected(messages_expected),
	answers(0),
	qos(0)
{
	request_topic = "fast/pscom/" + vm_name + "/any_proc/request";
	response_topic = "fast/pscom/" + vm_name + "/+/response";

	if (messages_expected > 0) {
		if (!(this->comm = std::dynamic_pointer_cast<fast::MQTT_communicator>(comm)))
			throw std::runtime_error("Suspending pscom procs is not available without MQTT_communicator.");
		// add subscription to response topic
		this->comm->add_subscription(response_topic, 0);
		// request shutdown
		suspend();
	}
}

Suspend_pscom::~Suspend_pscom()
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

void Suspend_pscom::suspend()
{
	if (messages_expected > 0) {
		std::string msg = "*";
		// publish suspend request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
	}
}

void Suspend_pscom::resume()
{
	// only try to resume if pscom is suspended
	if (answers == messages_expected && messages_expected > 0) {
		std::string msg = "*";
		// publish resume request
		comm->send_message(msg, request_topic, qos);
		// wait for termination
		for (answers = 0; answers != messages_expected; ++answers)
			comm->get_message(response_topic, std::chrono::seconds(10));
		// reset answers counter
		answers = 0;
	}
}
