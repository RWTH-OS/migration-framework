#ifndef MQTT_COMMUNICATOR_HPP
#define MQTT_COMMUNICATOR_HPP

#include "communicator.hpp"

#include <mosquittopp.h>

#include <string>

class MQTT_communicator : 
	public Communicator, 
	private mosqpp::mosquittopp
{
private:
	std::string id;
	std::string topic;
	std::string host;
	int port;
	int keepalive;

	void on_connect(int rc);
	void on_disconnect(int rc);
	void on_publish(int mid);
public:
	MQTT_communicator(const std::string &id, 
			  const std::string &topic, 
			  const std::string &host, 
			  int port);
	~MQTT_communicator();
	void send_message(const std::string &message);
};

#endif
