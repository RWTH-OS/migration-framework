#ifndef COMMUNICATOR_HPP
#define COMMUNICATOR_HPP

#include <string>

class Communicator
{
public:
	virtual ~Communicator(){};
	virtual void send_message(const std::string &message) = 0;
};

#endif
