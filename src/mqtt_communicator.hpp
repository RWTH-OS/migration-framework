#ifndef MQTT_COMMUNICATOR_HPP
#define MQTT_COMMUNICATOR_HPP

#include "communicator.hpp"

#include <mosquittopp.h>

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

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
	std::mutex msg_queue_mutex;
	std::condition_variable msg_queue_empty_cv;
	std::queue<mosquitto_message*> messages; /// TODO: Consider using unique_ptr.

	void on_connect(int rc);
	void on_disconnect(int rc);
	void on_publish(int mid);
	void on_message(const mosquitto_message *msg);
public:
	MQTT_communicator(const std::string &id, 
			  const std::string &topic, 
			  const std::string &host, 
			  int port,
			  int keepalive);
	~MQTT_communicator();
	void send_message(const std::string &message);
	std::string get_message();
};

#endif
