#ifndef MYSQL_SLAVE_H
#define MYSQL_SLAVE_H
#include "binlog_api.h"
#include "mysql.h"
#include <vector>
#include <map>
#include "process_memory.h"
#include <boost/smart_ptr.hpp>
#include "jsoncpp/json.h"
#include "CKafkaNode.h"

struct mysql_binlog_position
{
    uint64_t timestamp;
    uint64_t position;
    char filename[256];
};

class Incident_handler : public mysql::Content_handler
{
public:
    Incident_handler() : mysql::Content_handler() {}
    mysql::Binary_log_event *process_event(mysql::Incident_event *incident);
};

class Mysql_slave : public mysql::Content_handler
{
public:
    typedef std::vector<std::pair<std::string,std::string> >TypeColume;
     typedef std::map<std::string, TypeColume> TypeTable;
    typedef std::map<std::string, TypeTable> TypeDatabase;

    Mysql_slave ( mysql::system::Binary_log_driver *driver, const std::string &db_match_name, boost::shared_ptr<CKafkaProducer>&producer );
    virtual ~Mysql_slave ( );
    
    int build_db_structure();

    mysql::Binary_log_event *process_event ( mysql::Binary_log_event *event );

    void on_insert ( const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &fields );
    void on_update ( const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &old_fields, mysql::Row_of_fields &new_fields );
    void on_delete ( const std::string& db_name, const std::string& table_name, mysql::Row_of_fields &fields );

    static struct_memory<mysql_binlog_position> ipc_cluster;
    static std::vector<std::string> m_tables_filter;
    
    static bool table_mach(const std::string &tablename);
protected:
    bool is_text_field(mysql::Value &val);

private:
    mysql::system::Binlog_tcp_driver *m_log_driver;
    MYSQL m_mysql;
    TypeDatabase m_databases;
    std::string m_db_match_name;
    boost::shared_ptr<CKafkaProducer> m_producer;

};

#endif /* SLAVE_H */

