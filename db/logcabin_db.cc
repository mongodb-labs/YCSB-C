#include "logcabin_db.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
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
class ClusterPool {
public:
  ClusterPool(const std::string &host, int poolSize)
      : host(host),
        poolSize(poolSize) {
    for (int i = 0; i < poolSize; ++i) {
      pool.push(new LogCabin::Client::Cluster(host));
    }
  }

  ~ClusterPool() {
    while (!pool.empty()) {
      delete pool.front();
      pool.pop();
    }
  }

  LogCabin::Client::Cluster *checkout() {
    std::unique_lock<std::mutex> lock(mutex);
    while (pool.empty()) {
      condVar.wait(lock);
    }
    LogCabin::Client::Cluster *cluster = pool.front();
    pool.pop();
    return cluster;
  }

  void checkin(LogCabin::Client::Cluster *cluster) {
    std::unique_lock<std::mutex> lock(mutex);
    pool.push(cluster);
    condVar.notify_one();
  }

private:
  std::string host;
  size_t poolSize;
  std::queue<LogCabin::Client::Cluster *> pool;
  std::mutex mutex;
  std::condition_variable condVar;
};

LogCabinDB::LogCabinDB(const std::string &host, int poolSize)
    : clusterPool(new ClusterPool(host, poolSize)) // Initialize pool with 10 clusters
{}

int LogCabinDB::Read(const string &table, const string &key, const vector<string> *fields,
                     vector<KVPair> &result)
{
  // We're only prepared for workloads with readallfields=true.
  if (fields != nullptr)
  {
    throw invalid_argument("Can't yet handle workloads with readallfields=false");
  }

  auto cluster = clusterPool->checkout();
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
  clusterPool->checkin(cluster);
  return DB::kOK;
}

int LogCabinDB::Insert(const std::string &table, const std::string &key,
                       std::vector<KVPair> &values)
{
  auto valuesStr = toString(values);
#ifdef LOGCABIN_DB_VERBOSE
  cout << "INSERT table: " << table << ", key " << key << ", values " << valuesStr << endl;
#endif
  auto cluster = clusterPool->checkout();
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

  clusterPool->checkin(cluster);
  return DB::kOK;
}

int LogCabinDB::Delete(const string &table, const string &key) { return DB::kOK; }
} // namespace ycsbc
