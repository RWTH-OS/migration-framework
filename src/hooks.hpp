#ifndef HOOKS_HPP
#define HOOKS_HPP

#include <mosquittopp.h>

#include <string>

class Suspend_pscom : 
	private mosqpp::mosquittopp
{	
public:
	Suspend_pscom(const std::string &vm_name, unsigned int messages_expected);
	~Suspend_pscom();
	void suspend();
	void resume();
private:
	void on_message(const mosquitto_message *msg) override;

	const std::string vm_name;
	unsigned int messages_expected;
	volatile unsigned int answers;
};

#endif
