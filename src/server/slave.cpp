#include "slave.h"
#include <stdlib.h>
#include <time.h>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include "jsoncpp/json.h"
#include <algorithm> 
#include "algo.h"
#define LOG4Z_FORMAT_INPUT_ENABLE
#include "log4z.h"

struct_memory<mysql_binlog_position> Mysql_slave::ipc_cluster;
std::vector<std::string> Mysql_slave::m_tables_filter;

static std::string get_now_time()
{
    time_t tmr = time(NULL);
    struct tm *t = localtime(&tmr);
    char buf[128];
    sprintf(buf, "%d-%d-%d %d:%d:%d",
            t->tm_year + 1900,
            t->tm_mon + 1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec);

    return std::string(buf);
}


bool Mysql_slave::table_mach(const std::string &tablename)
{
    std::vector<std::string>::const_iterator it;
    for(it=Mysql_slave::m_tables_filter.begin();it!=Mysql_slave::m_tables_filter.end();++it)
    {
        if (algo::MatchingString(tablename.c_str(), (*it).c_str()))
        {
            return true;
        }
    }
    
    return false;
}

/***************************************************************/

mysql::Binary_log_event * Incident_handler::process_event(mysql::Incident_event *incident)
{
    std::cout << "Incident_handler::process_event() => Event type: "
            << mysql::system::get_event_type_str(incident->get_event_type())
            << " length: " << incident->header()->event_length
            << " next pos: " << incident->header()->next_position
            << std::endl;
    std::cout << "type= "
            << (unsigned) incident->type
            << " message= "
            << incident->message
            << std::endl
            << std::endl;
    /* Consume the event */
    delete incident;
    return 0;
}

/**************************************************************/
Mysql_slave::Mysql_slave(mysql::system::Binary_log_driver *driver,const std::string &db_match_name ,boost::shared_ptr<CKafkaProducer>&producer)
: mysql::Content_handler()
,m_log_driver(dynamic_cast<mysql::system::Binlog_tcp_driver*>(driver))
,m_db_match_name(db_match_name)
,m_producer(producer)
{
}

Mysql_slave::~Mysql_slave()
{
}

bool Mysql_slave::is_text_field(mysql::Value &val)
{
    if (val.type() == mysql::system::MYSQL_TYPE_VARCHAR ||
            val.type() == mysql::system::MYSQL_TYPE_BLOB ||
            val.type() == mysql::system::MYSQL_TYPE_MEDIUM_BLOB ||
            val.type() == mysql::system::MYSQL_TYPE_LONG_BLOB)
        return true;
    return false;
}

int Mysql_slave::build_db_structure()
{
    MYSQL_RES* res_db = 0;
    MYSQL_RES* res_tbl = 0;
    MYSQL_RES* res_column = 0;
    MYSQL_ROW row;

    if (!mysql_init(&m_mysql))
    {
        return -1;
    }

    if (mysql_real_connect(&m_mysql, m_log_driver->host().c_str(), m_log_driver->user().c_str(), m_log_driver->password().c_str(), 0, m_log_driver->port(), 0, 0) == 0)
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "mysql_real_connect() to '%s:%d' with user '%s' failed: %s",
                m_log_driver->host().c_str(), m_log_driver->port(), m_log_driver->user().c_str(), mysql_error(&m_mysql));
        return -1;
    }

    if ((res_db = mysql_list_dbs(&m_mysql, NULL)) == NULL)
    {
        LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID,"build_db_structure() call to 'show databases' failed: %s", mysql_error(&m_mysql));
        return -1;
    }

    while ((row = mysql_fetch_row(res_db)) != NULL)
    {
        m_databases[row[0]];
        if (mysql_select_db(&m_mysql, row[0]) || (res_tbl = mysql_list_tables(&m_mysql, NULL)) == NULL)
        {
            if (res_db)
            {
                mysql_free_result(res_db);
            }
            if (res_tbl)
            {
                mysql_free_result(res_tbl);
            }
            LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "build_db_structure() call to 'mysql_list_tables()' failed: %s", mysql_error(&m_mysql));
            return -1;
        }

        MYSQL_ROW row1;
        while ((row1 = mysql_fetch_row(res_tbl)) != NULL)
        {
            LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "WATCH:dbname=>%s,table=>%s", row[0],row1[0]);
            
            m_databases[row[0]][row1[0]];
            char query[256];
            sprintf(query, "SHOW COLUMNS FROM %s", row1[0]);

            if (mysql_query(&m_mysql, query) != 0 || (res_column = mysql_store_result(&m_mysql)) == NULL)
            {
                LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "build_db_structure() call to '%s' failed: %s", query, mysql_error(&m_mysql));
                if (res_db)
                {
                    mysql_free_result(res_db);
                }
                if (res_tbl)
                {
                    mysql_free_result(res_tbl);
                }
                if (res_column)
                {
                    mysql_free_result(res_tbl);
                }
                return -1;
            }

            size_t pos = 0;
            MYSQL_ROW row2;
            while ((row2 = mysql_fetch_row(res_column)) != NULL)
            {
               m_databases[row[0]][row1[0]].push_back(std::make_pair<std::string,std::string>(row2[0],row2[1]));
            }
            if(res_column)
            {
                mysql_free_result(res_column);
                res_column = NULL;
            }
        }
        mysql_free_result(res_tbl);
        res_tbl = NULL;
    }

    if (res_db)
    {
        mysql_free_result(res_db);
    }
    if (res_tbl)
    {
        mysql_free_result(res_tbl);
    }
    if (res_column)
    {
        mysql_free_result(res_tbl);
    }
    
    mysql_close(&m_mysql);
    
    return 0;
}

mysql::Binary_log_event * Mysql_slave::process_event(mysql::Binary_log_event *event)
{
    if (event->get_event_type() != mysql::USER_DEFINED)
        return event;

    mysql_binlog_position *pos = Mysql_slave::ipc_cluster[1];
    pos->timestamp = time(NULL);
    pos->position = event->header()->next_position;
    Mysql_slave::ipc_cluster.flush(1);
    
    mysql::Transaction_log_event *trans = static_cast<mysql::Transaction_log_event *> (event);

    /*
      The transaction event we created has aggregated all row events in an
      ordered list.
     */
    BOOST_FOREACH(mysql::Binary_log_event * event, trans->m_events)
    {
        switch (event->get_event_type())
        {
            case mysql::WRITE_ROWS_EVENT:
            case mysql::DELETE_ROWS_EVENT:
            case mysql::UPDATE_ROWS_EVENT:
            {
                mysql::Row_event *rev = static_cast<mysql::Row_event *> (event);
                boost::uint64_t table_id = rev->table_id;
                // BUG: will create a new event header if the table id doesn't exist.
                Binary_log_event * tmevent = trans->table_map()[table_id];
                mysql::Table_map_event *tm;
                if (tmevent != NULL)
                    tm = static_cast<mysql::Table_map_event *> (tmevent);
                else
                {
                    LOGFMT_ERROR(LOG4Z_MAIN_LOGGER_ID, "Table id %d was not registered by any preceding table map event.", table_id);
                    continue;
                }
                /*
                 Each row event contains multiple rows and fields. The Row_iterator
                 allows us to iterate one row at a time.
                 */

                if (algo::MatchingString(tm->db_name.c_str(), m_db_match_name.c_str()) && Mysql_slave::table_mach(tm->table_name))
                {
                    mysql::Row_event_set rows(rev, tm);
                    mysql::Row_event_set::iterator it = rows.begin();
                    do
                    {
                        mysql::Row_of_fields fields = *it;

                        if (event->get_event_type() == mysql::WRITE_ROWS_EVENT)
                            this->on_insert(tm->db_name, tm->table_name, fields);
                        if (event->get_event_type() == mysql::UPDATE_ROWS_EVENT)
                        {
                            ++it;
                            mysql::Row_of_fields fields2 = *it;
                            this->on_update(tm->db_name, tm->table_name, fields, fields2);
                        }
                        if (event->get_event_type() == mysql::DELETE_ROWS_EVENT)
                            this->on_delete(tm->db_name, tm->table_name, fields);
                    }
                    while (++it != rows.end());
                }
                break;
            }
            case mysql::QUERY_EVENT:
            {
                const mysql::Query_event *qev = static_cast<const mysql::Query_event *> (event);
                LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Execute SQL=>[%s]%s,position=>%u.", qev->db_name.c_str(), qev->query.c_str());
                break;
            }
            case mysql::ROTATE_EVENT:
            {
                mysql::Rotate_event *rot = static_cast<mysql::Rotate_event *> (event);
                pos->timestamp = time(NULL);
                pos->position = rot->binlog_pos;
                strcpy(pos->filename, rot->binlog_file.c_str());
                Mysql_slave::ipc_cluster.flush(1);
                
                LOGFMT_INFO(LOG4Z_MAIN_LOGGER_ID, "Binlog switch: file=>%s,position=>%u.", rot->binlog_file.c_str(), rot->binlog_pos);
                break;
            }
            default:
            {
                break;
            }
        } // end switch
    } // end BOOST_FOREACH
    /* Consume the event */
    delete trans;
    return 0;
}

void Mysql_slave::on_insert(const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &fields)
{
    Json::Value text_json;
    text_json["dbname"] = db_name;
    text_json["operator"] = "insert";
    text_json["table"] = table_name;
    text_json["time"] = get_now_time();
            
    mysql::Row_of_fields::iterator field_it = fields.begin();
    mysql::Converter converter;
    Json::Value test_row_json;
    int field_id = 0;
    do
    {
        std::string str;
        converter.to(str, *field_it);

        std::string col_name = (m_databases[db_name][table_name][field_id++]).first;
        std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
        test_row_json[col_name] = str;
        
        ++field_it;
    }
    while (field_it != fields.end());

    text_json["data"] = test_row_json;
    std::string strmsg = Json::FastWriter().write(text_json);
    m_producer->Send(strmsg);
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "INSERT:%s", strmsg.c_str());
}

void Mysql_slave::on_update(const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &old_fields, mysql::Row_of_fields &new_fields)
{   
    Json::Value text_json;
    text_json["dbname"] = db_name;
    text_json["operator"] = "update";
    text_json["table"] = table_name;
    text_json["time"] = get_now_time();
    
    int field_id = 0;
    mysql::Row_of_fields::iterator field_it = new_fields.begin();
    mysql::Converter converter;
    Json::Value test_row_json;
    do
    {
        std::string str;
        converter.to(str, *field_it);

        std::string col_name = (m_databases[db_name][table_name][field_id++]).first;
        std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
        test_row_json["after"][col_name] = str;
                
        ++field_it;
    }
    while (field_it != new_fields.end());

    field_it = old_fields.begin();
    field_id = 0;
    do
    {
        std::string str;
        converter.to(str, *field_it);
        
                std::string col_name = (m_databases[db_name][table_name][field_id++]).first;
        std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
        test_row_json["befor"][col_name] = str;
        
        ++field_it;
    } while (field_it != old_fields.end());

    text_json["data"] = test_row_json;
    std::string strmsg = Json::FastWriter().write(text_json);
    m_producer->Send(strmsg);
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "UPDATE:%s", strmsg.c_str());
}

void Mysql_slave::on_delete(const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &fields)
{   
    Json::Value text_json;
    text_json["dbname"] = db_name;
    text_json["operator"] = "delete";
    text_json["table"] = table_name;
    text_json["time"] = get_now_time();

    mysql::Row_of_fields::iterator field_it = fields.begin();
    int field_id = 0;
    mysql::Converter converter;
    Json::Value test_row_json;
    do
    {
        std::string str;
        converter.to(str, *field_it);

        std::string col_name = (m_databases[db_name][table_name][field_id++]).first;
        std::transform(col_name.begin(), col_name.end(), col_name.begin(), ::tolower);
        test_row_json[col_name] = str;

        ++field_it;
    }  while (field_it != fields.end());

    text_json["data"] = test_row_json;
    std::string strmsg = Json::FastWriter().write(text_json);
    m_producer->Send(strmsg);
    LOGFMT_TRACE(LOG4Z_MAIN_LOGGER_ID, "DELETE:%s", strmsg.c_str());
}