#include "hooks.hpp"

#include <stdexcept>

Suspend_pscom::Suspend_pscom(const std::string &vm_name, unsigned int messages_expected) :
	vm_name(vm_name),
	messages_expected(messages_expected),
	answers(0)
{
	// init mosquitto
	loop_start();
	connect_async("localhost", 1883, 60);
	subscribe(nullptr, (vm_name + "_migration_resp").c_str(), 0);
	// request shutdown
	suspend();
}

Suspend_pscom::~Suspend_pscom()
{
	try {
		// request resume
		resume();
		// clear mosquitto
		disconnect();
		loop_stop();
	} catch (...) {
	}
}

void Suspend_pscom::on_message(const mosquitto_message *msg)
{
	(void)msg; // do nothing to suppress unused variable warning
	__sync_add_and_fetch(&answers, 1);
}


void Suspend_pscom::suspend()
{
	if (messages_expected > 0) {
		std::string topic = vm_name + "_migration_req";
		std::string msg = "*";
		// publish suspend request
		int ret = publish(nullptr, topic.c_str(), msg.size(), msg.c_str(), 0, false);
		if (ret != MOSQ_ERR_SUCCESS)
			throw std::runtime_error("Error sending suspend message: Code " + std::to_string(ret));
		// wait for termination
		while (answers != messages_expected);
	}
}

void Suspend_pscom::resume()
{
	// only try to resume if pscom is suspended
	if (answers == messages_expected && messages_expected > 0) {
		answers = 0;
		std::string topic = vm_name + "_migration_req";
		std::string msg = "*";
		// publish resume request
		int ret = publish(nullptr, topic.c_str(), msg.size(), msg.c_str(), 0, false);
		if (ret != MOSQ_ERR_SUCCESS)
			throw std::runtime_error("Error sending resume message: Code " + std::to_string(ret));
		// wait for termination
		while (answers != messages_expected);
		// reset answers counter
		answers = 0;
	}
}
