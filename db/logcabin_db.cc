#include "logcabin_db.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace std;

namespace
{
const uint64_t TIMEOUT_NANOS = 10ULL * 1000ULL * 1000ULL * 1000ULL;
typedef LogCabin::Client::Status Status;
typedef ycsbc::DB::KVPair KVPair;

ostream &operator<<(ostream &os, const vector<KVPair> &vec)
{
  os << "{";
  for (size_t i = 0; i < vec.size(); ++i)
  {
    os << "'" << vec[i].first << "': '" << vec[i].second << "'";
    if (i < vec.size() - 1)
    {
      os << ", ";
    }
  }
  os << "}";
  return os;
}

template <typename T> string toString(const T &t)
{
  stringstream ss;
  ss << t;
  return ss.str();
}
} // anonymous namespace

namespace ycsbc
{
LogCabinDB::LogCabinDB(const std::string& host)
    : cluster(new LogCabin::Client::Cluster(host))
{
}

int LogCabinDB::Read(const string &table, const string &key, const vector<string> *fields,
                     vector<KVPair> &result)
{
  // We're only prepared for workloads with readallfields=true.
  if (fields != nullptr)
  {
    throw invalid_argument("Can't yet handle workloads with readallfields=false");
  }

  auto tree = cluster->getTree();
#ifdef LOGCABIN_DB_TIMEOUT
  tree.setTimeout(TIMEOUT_NANOS);
#endif
  auto path = table + "/" + key;
  string contents;
  auto readResult = tree.read(path, contents);
  if (readResult.status != Status::OK) {
    cerr << "read: " << readResult.error << endl;
    exit(1);
  }
#ifdef LOGCABIN_DB_VERBOSE
  cout << "READ table: " << table << ", key " << key << ", fields NULL, " << contents << endl;
#endif
  // For benchmarking, don't actually parse 'contents' into 'result'.
  return DB::kOK;
}

int LogCabinDB::Insert(const std::string &table, const std::string &key,
                       std::vector<KVPair> &values)
{
  auto valuesStr = toString(values);
#ifdef LOGCABIN_DB_VERBOSE
  cout << "INSERT table: " << table << ", key " << key << ", values " << valuesStr << endl;
#endif
  auto tree = cluster->getTree();
#ifdef LOGCABIN_DB_TIMEOUT
  tree.setTimeout(TIMEOUT_NANOS);
#endif
  auto path = table + "/" + key;
  auto result = tree.makeDirectory(table);
  if (result.status != Status::OK)
  {
    cerr << "makeDirectory: " << result.error << endl;
    exit(1);
  }

  result = tree.write(path, valuesStr);
  if (result.status != Status::OK)
  {
    cerr << "write: " << result.error << endl;
    exit(1);
  }

  return DB::kOK;
}

int LogCabinDB::Delete(const string &table, const string &key) { return DB::kOK; }
} // namespace ycsbc
