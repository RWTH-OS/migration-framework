/*
 * This file is part of migration-framework.
 * Copyright (C) 2015 RWTH Aachen University - ACS
 *
 * This file is licensed under the GNU Lesser General Public License Version 3
 * Version 3, 29 June 2007. For details see 'LICENSE.md' in the root directory.
 */

#ifndef MQTT_COMMUNICATOR_HPP
#define MQTT_COMMUNICATOR_HPP

#include "communicator.hpp"

#include <mosquittopp.h>

#include <string>
#include <mutex>
#include <condition_variable>
#include <queue>

/**
 * \brief A specialized Communicator to provide communication using mqtt framework mosquitto.
 */
class MQTT_communicator : 
	public Communicator, 
	private mosqpp::mosquittopp
{
public:
	/**
	 * \brief Constructor for MQTT_communicator.
	 *
	 * Initializes mosquitto, establishes a connection, starts async mosquitto loop and subscribes to topic.
	 * The async mosquitto loop runs in a seperate thread so callbacks and send_/get_message should be threadsafe.
	 * \param id
	 * \param subscribe_topic The topic to subscribe to.
	 * \param publish_topic The topic to publish messages to.
	 * \param host The host to connect to.
	 * \param port The port to connect to.
	 * \param keepalive The number of seconds the broker sends periodically ping messages to test if client is still alive.
	 */
	MQTT_communicator(const std::string &id, 
			  const std::string &subscribe_topic,
			  const std::string &publish_topic,
			  const std::string &host, 
			  int port,
			  int keepalive);
	/**
	 * \brief Destructor for MQTT_communicator.
	 *
	 * Stops async mosquitto loop, disconnects and calls mosquitto lib cleanup.
	 */
	~MQTT_communicator();
	/**
	 * \brief Send a message.
	 */
	void send_message(const std::string &message) override;
	/**
	 * \brief Send a message.
	 */
	void send_message(const std::string &message, const std::string &topic);
	/**
	 * \brief Get a message.
	 */
	std::string get_message() override;

	/**
	 * \brief Get a message.
	 *
	 * \param duration The time in milliseconds until timeout.
	 */
	std::string get_message(const std::chrono::duration<double> &duration);
private:
	/**
	 * \brief Callback for established connections.
	 */
	void on_connect(int rc) override;
	/**
	 * \brief Callback for disconnected connections.
	 */
	void on_disconnect(int rc) override;
	/**
	 * \brief Callback for received messages.
	 */
	void on_message(const mosquitto_message *msg) override;

	std::string id;
	std::string subscribe_topic;
	std::string publish_topic;
	std::string host;
	int port;
	int keepalive;
	std::mutex msg_queue_mutex;
	std::condition_variable msg_queue_empty_cv;
	std::queue<mosquitto_message*> messages; /// \todo Consider using unique_ptr.

};

#endif
