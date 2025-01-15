#ifndef YCSB_C_LOGCABIN_DB_H_
#define YCSB_C_LOGCABIN_DB_H_

#include "core/db.h"

#include "LogCabin/Client.h"

#include <iostream>
#include <memory>
#include <string>

namespace ycsbc
{
class LogCabinDB : public DB
{
public:
  LogCabinDB(const std::string& host);

  int Read(const std::string &table, const std::string &key, const std::vector<std::string> *fields,
           std::vector<KVPair> &result);

  int Scan(const std::string &table, const std::string &key, int len,
           const std::vector<std::string> *fields, std::vector<std::vector<KVPair>> &result)
  {
    throw "Scan: function not implemented!";
  }

  int Update(const std::string &table, const std::string &key, std::vector<KVPair> &values)
  {
    throw "Update: function not implemented!";
  }

  int Insert(const std::string &table, const std::string &key, std::vector<KVPair> &values);

  int Delete(const std::string &table, const std::string &key);

private:
  std::unique_ptr<LogCabin::Client::Cluster> cluster;
};

} // ycsbc

#endif // YCSB_C_LOGCABIN_DB_H_
