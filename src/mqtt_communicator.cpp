#include "mqtt_communicator.hpp"

#include "logging.hpp"

#include <stdexcept>
#include <cstdlib>

MQTT_communicator::MQTT_communicator(const std::string &id, 
				     const std::string &topic,
				     const std::string &host, 
				     int port) :
	mosqpp::mosquittopp(id.c_str()),
	id(id), 
	topic(topic),
	host(host), 
	port(port),
	keepalive(60)
{
	LOG_PRINT(LOG_NOTICE, "Initializing MQTT_communicator...");
	/// TODO: Call mutex constructors instead of lock/unlock.
	empty_mutex.lock();
	msg_queue_mutex.unlock();
	mosqpp::lib_init();
	connect_async(host.c_str(), port, keepalive);
	loop_start();
	/// TODO: Move subscribe to a dedicated public method.
	subscribe(nullptr, "topic1", 2);
	LOG_PRINT(LOG_NOTICE, "MQTT_communicator initialized.");
}

MQTT_communicator::~MQTT_communicator()
{
	LOG_PRINT(LOG_NOTICE, "Closing MQTT_communicator...");
	disconnect();
	loop_stop();
	mosqpp::lib_cleanup();
	LOG_PRINT(LOG_NOTICE, "MQTT_communicator closed.");
}

void MQTT_communicator::on_connect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Connection established to " + host + ":" + std::to_string(port)).c_str());
	} else {
		LOG_PRINT(LOG_ERR, ("Error on connect: Code " + std::to_string(rc)).c_str());
	}
}

void MQTT_communicator::on_disconnect(int rc)
{
	if (rc == 0) {
		LOG_PRINT(LOG_NOTICE, ("Disconnected from  " + host + ":" + std::to_string(port)).c_str());
	} else {
		LOG_PRINT(LOG_ERR, ("Unexpected disconnect: Code " + std::to_string(rc)).c_str());
	}
}

void MQTT_communicator::on_publish(int mid)
{
	LOG_PRINT(LOG_NOTICE, ("Message " + std::to_string(mid) + " published.").c_str());
}

void MQTT_communicator::on_message(const mosquitto_message *msg)
{
	LOG_PRINT(LOG_NOTICE, "on_message executed.");
	msg_queue_mutex.lock();
	if (messages.empty())
		empty_mutex.unlock();
	/// TODO: Add error handling.
	messages.push((mosquitto_message*)malloc(sizeof(mosquitto_message)));
	mosquitto_message_copy(messages.back(), msg);
	msg_queue_mutex.unlock();
	LOG_PRINT(LOG_NOTICE, "Message added to queue.");
}

void MQTT_communicator::send_message(const std::string &message)
{
	int ret = publish(nullptr, topic.c_str(), message.size(), message.c_str(), 2, false);
	if (ret != MOSQ_ERR_SUCCESS)
		throw std::runtime_error("Error sending message: Code " + std::to_string(ret));
}

std::string MQTT_communicator::get_message()
{
	LOG_PRINT(LOG_NOTICE, "Locking empty_mutex");
	empty_mutex.lock();
	LOG_PRINT(LOG_NOTICE, "Locking msg_queue_mutex");
	msg_queue_mutex.lock();
	LOG_PRINT(LOG_NOTICE, "Get first msg.");
	mosquitto_message *msg = messages.front();
	LOG_PRINT(LOG_NOTICE, "Pop from queue.");
	messages.pop();
	if(!messages.empty())
		empty_mutex.unlock();
	msg_queue_mutex.unlock();
	LOG_PRINT(LOG_NOTICE, (std::string("Topic: ") + msg->topic).c_str());
	LOG_PRINT(LOG_NOTICE, "Converting payload to string.");
	/// TODO: Find better c++ style solution to convert payload to string.
	char* buf = new char[msg->payloadlen+1];
	buf[msg->payloadlen] = '\0';
	memcpy(buf, msg->payload, msg->payloadlen);
	std::string payload(buf);
	delete[] buf;
	LOG_PRINT(LOG_NOTICE, "Free msg.");
	mosquitto_message_free(&msg);
	LOG_PRINT(LOG_NOTICE, "Returning payload string.");
	return payload;
}
