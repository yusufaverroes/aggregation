#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>
#include <map>
#include <set>
//#include <opencv2/opencv.hpp>

#include <unistd.h>
#include <websocketpp/common/thread.hpp>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;

using websocketpp::lib::condition_variable;
using websocketpp::lib::lock_guard;
using websocketpp::lib::mutex;
using websocketpp::lib::thread;
using websocketpp::lib::unique_lock;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */

enum action_type
{
    SUBSCRIBE,
    UNSUBSCRIBE,
    MESSAGE
};

struct action
{
    action(action_type t, connection_hdl h) : type(t), hdl(h) {}
    action(action_type t, connection_hdl h, server::message_ptr m)
        : type(t), hdl(h), msg(m) {}

    action_type type;
    websocketpp::connection_hdl hdl;
    server::message_ptr msg;
};

class broadcast_server
{
public:
    broadcast_server()
    {
        // Initialize Asio Transport
        m_server.init_asio();

        // Register handler callbacks
        m_server.set_open_handler(bind(&broadcast_server::on_open, this, ::_1));
        m_server.set_close_handler(bind(&broadcast_server::on_close, this, ::_1));
        m_server.set_message_handler(bind(&broadcast_server::on_message, this, ::_1, std::placeholders::_2));
    }

    void run(uint16_t port)
    {
        // listen on specified port
        m_server.listen(port);

        // Start the server accept loop
        m_server.start_accept();

        // Start the ASIO io_service run loop
        try
        {
            m_server.run();
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    void on_open(connection_hdl hdl)
    {
        server::connection_ptr con = m_server.get_con_from_hdl(hdl);
        std::string client_id = con->get_request_header("Client-ID");

        {
            lock_guard<mutex> guard(m_action_lock);
            m_connections[hdl] = client_id; // Associate the client's connection handle with the provided unique identifier
            std::cout << "Client connected with ID: " << client_id << std::endl;
        }

        m_action_cond.notify_one();
    }

    void on_close(connection_hdl hdl)
    {
        {
            
            lock_guard<mutex> guard(m_action_lock);
            m_connections.erase(hdl);
            std::cout << "Client disconnected, connection id : "<< m_connections[hdl] << std::endl;
        }

        m_action_cond.notify_one();
    }

    void on_message(connection_hdl hdl, server::message_ptr msg)
    {
        // queue message up for sending by processing thread
        {
            lock_guard<mutex> guard(m_action_lock);
            // std::cout << "on_message" << std::endl;
            m_actions.push(action(MESSAGE, hdl, msg));
        }
        m_action_cond.notify_one();
    }
    void looping(){
            while(1){
            if (!stop_streaming)
            {
                if (b!=NULL && !b->hdl.expired()){
                    con_list::iterator it = m_connections.find(b->hdl);
                    
                    if (it != m_connections.end())
                    {
                        std::string client_id = it->second;
                        if (client_id == "client1" && streaming_to_client1)
                        {   //get image data here
                            m_server.send(b->hdl, "a", websocketpp::frame::opcode::text);
                        }
                        else if (client_id == "client2" && streaming_to_client2)
                        {
                            if (client2_data_count < max_client2_data_count)
                            {
                                //get string data here
                                m_server.send(b->hdl, "b", websocketpp::frame::opcode::text);
                                client2_data_count++;
                            }
                            else
                            {
                                streaming_to_client2 = false;
                                streaming_to_client1 = true;
                            }
                        }
                    }
                }
                else
                {
                    streaming_to_client1 = false;
                    streaming_to_client2 = false;
                    stop_streaming = false;
                }
            }
            usleep(1000000);    
        }
    }

    void process_messages()
    {
        while (1)
        {

            unique_lock<mutex> lock(m_action_lock);

            while (m_actions.empty())
            {
                m_action_cond.wait(lock);
            }

            action a = m_actions.front();

            m_actions.pop();
            b = &a;
            lock.unlock();

            if (a.type == SUBSCRIBE)
            {
                // Handle client subscription
            }
            else if (a.type == UNSUBSCRIBE)
            {
                // Handle client unsubscription
            }
            else if (a.type == MESSAGE)
            {
                std::string message = a.msg->get_payload();

                if (message == "stop_streaming")
                {
                    stop_streaming = true;
                }
                else if (message == "get_data")
                {
                    streaming_to_client1 = false;
                    streaming_to_client2 = true;
                    client2_data_count = 0;
                }else if (message == "start_streaming")
                {
                    stop_streaming=false;
                    streaming_to_client1 = true;
                }
            }

            // if (!stop_streaming) // TODO : spllit into another thread from here, to make looping
            // {
            //     con_list::iterator it = m_connections.find(a.hdl);
            //     if (it != m_connections.end())
            //     {
            //         std::string client_id = it->second;
            //         if (client_id == "client1" && streaming_to_client1)
            //         {
            //             m_server.send(a.hdl, "a", websocketpp::frame::opcode::text);
            //         }
            //         else if (client_id == "client2" && streaming_to_client2)
            //         {
            //             if (client2_data_count < max_client2_data_count)
            //             {
            //                 m_server.send(a.hdl, "b", websocketpp::frame::opcode::text);
            //                 client2_data_count++;
            //             }
            //             else
            //             {
            //                 streaming_to_client2 = false;
            //                 streaming_to_client1 = true;
            //             }
            //         }
            //     }
            // }
            // else
            // {
            //     streaming_to_client1 = false;
            //     streaming_to_client2 = false;
            //     stop_streaming = false;
            // }
        }
    }

private:
    typedef std::map<connection_hdl, std::string, std::owner_less<connection_hdl>> con_list;

    server m_server;
    con_list m_connections;
    std::queue<action> m_actions;

    mutex m_action_lock;
    mutex m_connection_lock;
    condition_variable m_action_cond;
    action* b;

    bool streaming_to_client1 = true;
    bool streaming_to_client2 = false;
    bool stop_streaming = false;

    int client2_data_count = 0;
    const int max_client2_data_count = 3;
};

int main()
{
    try
    {
        broadcast_server server_instance;

        // Start a thread to run the processing loop
        thread t(bind(&broadcast_server::process_messages, &server_instance));
        thread l(bind(&broadcast_server::looping, &server_instance));

        // Run the ASIO io_service with the main thread
        server_instance.run(9002);

        // Wait for the processing thread to finish
        t.join();
        l.join();
    }
    catch (websocketpp::exception const &e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}