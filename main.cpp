/****************************************************************************
 * 
 * Copyright (c) 2023, libmav development team
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *    the documentation and/or other materials provided with the 
 *    distribution.
 * 3. Neither the name libmav nor the names of itsl contributors may be
 *    used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 ****************************************************************************/


#include <iostream>

#include <mav/MessageSet.h>
#include <mav/Message.h>
#include <mav/Network.h>
#include <mav/UDPServer.h>


int main(int argc, char** argv) {
    /*
     * libmav does not include any messages. All messages to be used are just loaded from an
     * XML file. This project contains the official mavlink repo as a submodule, so we can just
     * use the development.xml file from there.
     */
    mav::MessageSet message_set{"mavlink/development.xml"};
    /*
     * We connect to PX4 SITL here. You may wonder why we use UDPServer and not UDPClient here. MAVLink works in
     * the opposite order of what you'd expect here, in fact, PX4 SITL just sends a mavlink stream to 127.0.0.1:14550,
     * no matter if someone is listening there or not. So we'll have to put a server to listen on this port
     * to receive the stream. The UDPClient you would use in case you had a real mavlink UDP server listening on
     * known port.
     */
    mav::UDPServer phy{14550};

    /*
     * Here, we define the heartbeat that we want to send out ourselves. We can change the heartbeat
     * later by calling net.setHeartbeat(...). The heartbeat is the only message that is sent out automatically,
     * all other messages have to be sent manually.
     * You do not have to set a HEARTBEAT message at all, in that case, the interface will just be eavesdropping.
     */
    auto own_heartbeat = message_set.create("HEARTBEAT").set({
        {"type", message_set.e("MAV_TYPE_GCS")},
        {"autopilot", message_set.e("MAV_AUTOPILOT_INVALID")},
        {"base_mode", message_set.e("MAV_MODE_FLAG_CUSTOM_MODE_ENABLED")},
        {"custom_mode", 0},
        {"system_status", message_set.e("MAV_STATE_ACTIVE")}
    });

    /*
     * The NetworkRuntime class is the only class that spawns any threads in libmav. It is responsible
     * for driving the network interface. You can omit the heartbeat message here, if you just want to eavesdrop.
     * Libmav does not care about "mavlink routing", it sorts connections by their real network connections.
     * Therefore, system id and component id have little meaning for libmav and it assigns a default one to be used
     * by this interface. You can set a custom one as the first argument in case you'd like to modify it.
     */
    mav::NetworkRuntime net{message_set, own_heartbeat, phy};

    /*
     * For libmav, having a "connection" means to have received a mavlink message from a unique network idenfifier
     * (IP address and port, or serial port). For the "client" interface types (TCPClient, UDPClient, Serial) there
     * can only be one "connection", as they can only connect to a single IP / Port. For the "server" interface types
     * (TCPServer, UDPServer) there can be multiple connections, as multiple clients can connect to them.
     * For those, awaitConnections makes less sense, there you'd rather use the onConnection callback.
     * awaitConnection waits for the first connection (reception of mavlink data) and returns the connection object.
     */
    std::shared_ptr<mav::Connection> connection = net.awaitConnection(200);

    /*
     * We can now send and receive messages using the connection object. The connection object is thread-safe.
     * You can add a general async callback to the connection object, which will be called for every message
     * received on this connection.
     * In general, it is easier to use the synchronous API to send / receive specific messages.
     * Here, as an example, we receive 5 consecutive HEARTBEAT messages. The timeout parameter is optional,
     * however, if you don't set one, the thread will potentially block forever if no message is received.
     */
    for (int i=0; i<5; i++) {
        auto message = connection->receive("HEARTBEAT", 5000);
        std::cout << "Received HEARTBEAT #" << i << std::endl;
    }

    /*
     * Here, we show how one can perform a transaction over mavlink. We first specify what we expect to
     * receive, once we ask for it. This makes the library aware to listen to a specific answer. Then we send out
     * the request, and then wait for our expectation to be fulfilled (or timeout). The response is then returned.
     * Technically it would also work most of the time if we just did send first and then receive without the
     * expectation, but that would be a potential race condition. With the expectation setting first, we can
     * guarantee that we will not miss the answer.
     */
    auto expectation = connection->expect("AUTOPILOT_VERSION");
    connection->send(message_set.create("COMMAND_LONG").set({
        {"command", message_set.e("MAV_CMD_REQUEST_MESSAGE")},
        {"param1", message_set.idForMessage("AUTOPILOT_VERSION")},
        {"target_system", 1},
        {"target_component", 1},
        {"param7", 1}
    }));
    auto response = connection->receive(expectation, 1000);

    std::cout << "Received AUTOPILOT_VERSION" << std::endl;

    /*
     * libmav automatically extracts fields from the message using the correct encoding. In this case, "product_id"
     * is a uint16, which libmav will always automatically extract correctly as a uint16. However, since this is a
     * numeric type, you can assign it to any numeric type and automatic type conversion will be performed, in this
     * case to a int32.
     */
    int product_id = response["product_id"];
    int vendor_id = response["vendor_id"];

    /*
     * You can directly assign array fields to iterable array types, such as std::vector or std::array.
     */
    std::vector<uint8_t> uid_bytes = response["uid"];

    /*
     * You still have to be a bit careful, while it is of course perfectly safe to extract all numeric types < 32 bits
     * as a 32 bit integer, you have to assign 64bit integers to the correct type to not lose precision.
     */
    uint64_t uid = response["uid"];

    std::cout << "Product ID: " << product_id << std::endl;
    std::cout << "Vendor ID: " << vendor_id << std::endl;
    std::cout << "UID: " << uid << std::endl;

    /*
     * Here, the compiler can not infer what type we'd like "flight_sw_version" to be once it is extracted, that's
     * because we don't assign it to a variable, but pipe it to std::cout which itself can take any type of variable.
     * Here, we can explicitly cast the extracted value to the type we'd like it to be.
     */
    std::cout << response["flight_sw_version"].as<int>() << std::endl;


    /*
     * In this example, we request a param value from the Autpilot. The transaction is simular to the example above
     */
    expectation = connection->expect("PARAM_VALUE");

    /*
     * While in the previous example we used the inititalizer list API, here we set each individual field.
     * Note that you can directly assign strings to char array fields and also assign any numeric type to any
     * numeric field. The library will automatically convert it to the correct type.
     */
    auto request = message_set.create("PARAM_REQUEST_READ");
    request["param_index"] = -1;
    request["param_id"] = "SYS_AUTOSTART";
    request["target_system"] = 1;
    request["target_component"] = 1;
    connection->send(request);

    response = connection->receive(expectation, 1000);

    std::cout << "Received PARAM_VALUE" << std::endl;

    /*
     * You can directly assign char arrays to std::strings
     */
    std::string param_id = response["param_id"];

    /*
     * This is another mavlink oddity: In some protocols, float fields are used to transmit 32-bit integers.
     * Since libmav sees this value as a float value, you have to explicitly tell it to unpack the bytes as an integer.
     * Note that this is different than doing response["param_value"].as<int>(), which would extract the float value
     * and then cast it to an integer, which would be wrong in this case.
     */
    int param_value = response["param_value"].floatUnpack<int>();

    std::cout << "Param ID: " << param_id << std::endl;
    std::cout << "Param Value: " << param_value << std::endl;
    return 0;
}
