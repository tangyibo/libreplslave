#include <stdlib.h>
#include <stdio.h>
#include <fstream>
#include "slave.h"
#define LOG4Z_FORMAT_INPUT_ENABLE
#include "log4z.h"
    
int main(int argc, char** argv)
{
    using namespace zsummer::log4z;

    ILog4zManager::getRef().config("./logger.conf");
    ILog4zManager::getRef().start();
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Start server ...");

    std::ifstream infile("./config.json");
    Json::Reader reader;
    Json::Value json_root;
    if (!reader.parse(infile, json_root) || json_root.empty())
    {
        fprintf(stderr, "parse configure file content failed.\n");
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "parse configure file content failed!");
        return (EXIT_FAILURE);
    }

    Json::Value mysql_root = json_root["mysql"];
    Json::Value kafka_root = json_root["kafka"];

    //connect kafka
    const char *brokers = kafka_root["brokers"].asCString();
    const char *topic = kafka_root["topic"].asCString();

    //connect mysql
    std::string connstr = mysql_root["connstr"].asString();
    std::string dbname = mysql_root["dbname"].asString();
    Json::Value tablelist=json_root["tables"];
    for (int i = 0; i < tablelist.size(); ++i)
    {
        Mysql_slave::m_tables_filter.push_back(tablelist[i].asString());
    }


    if (0 != Mysql_slave::ipc_cluster.load("./media.map", 1, true))
    {
        fprintf(stderr, "Can't open file media.map file.\n");
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Can't open file media.map file.");
        return (EXIT_FAILURE);
    }
    
    //connect kafka
    boost::shared_ptr<CKafkaProducer> producer;
    producer.reset(new CKafkaProducer());
    if(!producer->Init(brokers,topic))
    {
        fprintf(stderr, "Connect Kafka failed.\n");
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Connect Kafka failed.");
        return (EXIT_FAILURE);
    }
    
    mysql::system::Binary_log_driver *driver=mysql::system::create_transport(connstr.c_str());
    mysql::Binary_log binlog(driver);

    /*
      Attach a custom event parser which produces user defined events
     */
    mysql::Basic_transaction_parser transaction_parser;
    //Incident_handler incident_hndlr;
    Mysql_slave slave(driver,dbname,producer);

    binlog.content_handler_pipeline()->push_back(&transaction_parser);
    //binlog.content_handler_pipeline()->push_back(&incident_hndlr);
    binlog.content_handler_pipeline()->push_back(&slave);

    if (0 != slave.build_db_structure())
        return (EXIT_FAILURE);

    if (binlog.connect())
    {
        fprintf(stderr, "Can't connect to the master.\n");
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Can't connect to the master.");
        return (EXIT_FAILURE);
    }

    std::string binfile;
    unsigned long position = 0;

    position = binlog.get_position(binfile);
    mysql_binlog_position *pos = Mysql_slave::ipc_cluster[1];
    if (pos->timestamp <= 0)
    {
        pos->timestamp = time(NULL);
        pos->position = position;
        strcpy(pos->filename, binfile.c_str());
        Mysql_slave::ipc_cluster.flush(1);
    }
    else
    {
        binfile.assign(pos->filename);
        position = pos->position-1;
        
        if (ERR_OK != binlog.set_position(binfile, position))
        {
            fprintf(stderr, "Can't connect to the master.\n");
            LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Can't connect to the master.");
            return (EXIT_FAILURE);
        }

        LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Read postion from media.map file!");
    }
   
    Binary_log_event *event;

    bool quit = false;
    while (!quit)
    {
        /*
         Pull events from the master. This is the heart beat of the event listener.
         */
        if (binlog.wait_for_next_event(&event))
        {
            quit = true;
            continue;
        }

//        /*
//         Perform a special action based on event type
//         */
//        switch (event->header()->type_code)
//        {
//            case mysql::QUERY_EVENT:
//            {
//                const mysql::Query_event *qev = static_cast<const mysql::Query_event *> (event);
//                LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Execute SQL=>[%s]%s,position=>%u.", qev->db_name.c_str(), qev->query.c_str());
//                break;
//            }
//            case mysql::ROTATE_EVENT:
//            {
//                mysql::Rotate_event *rot = static_cast<mysql::Rotate_event *> (event);
//                pos->timestamp = time(NULL);
//                pos->position = rot->binlog_pos;
//                strcpy(pos->filename, rot->binlog_file.c_str());
//                Mysql_slave::ipc_cluster.flush(1);
//
//                LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Binlog switch: file=>%s,position=>%u.", rot->binlog_file.c_str(), rot->binlog_pos);
//                break;
//            }
//            default:
//                break;
//        } // end switch
        
        delete event;
    } // end loop
    
    LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Server is stoped ...");
    
    return (EXIT_SUCCESS);
}
