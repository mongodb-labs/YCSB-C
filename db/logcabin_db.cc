#include "logcabin_db.h"

#include <iostream>
#include <regex>
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

void parseStringToVector(const string &input, vector<KVPair> &result)
{
  // Ensure the input starts and ends with curly braces
  if (input.front() != '{' || input.back() != '}')
  {
    throw invalid_argument(
      "Input string must be enclosed in curly braces: " + input);
  }

  // Extract the content inside the curly braces
  string content = input.substr(1, input.size() - 2);

  // Regular expression to match key-value pairs
  // TODO: no regex
  regex kv_regex(R"(\s*'([^']+)'\s*:\s*'([^']+)'\s*)");
  auto kv_begin = sregex_iterator(content.begin(), content.end(), kv_regex);
  auto kv_end = sregex_iterator();

  // Iterate over all matches
  for (auto it = kv_begin; it != kv_end; ++it)
  {
    smatch match = *it;
    string key = match[1].str();
    string value = match[2].str();
    result.emplace_back(key, value);
  }
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
  tree.setTimeout(TIMEOUT_NANOS);
  auto path = table + "/" + key;
  string contents;
  auto readResult = tree.read(path, contents);
  if (readResult.status != Status::OK) {
    cerr << "read: " << readResult.error << endl;
    exit(1);
  }
  parseStringToVector(contents, result);
  cout << "READ table: " << table << ", key " << key << ", fields NULL, " << result << endl;
  return DB::kOK;
}

int LogCabinDB::Update(const string &table, const string &key, vector<KVPair> &values)
{
  auto valuesStr = toString(values);
  cout << "UPDATE table: " << table << ", key " << key << ", values " << valuesStr << endl;
  auto tree = cluster->getTree();
  tree.setTimeout(TIMEOUT_NANOS);
  auto path = table + "/" + key;
  string contents;
  auto readResult = tree.read(path, contents);
  if (readResult.status != Status::OK) {
    cerr << "read: " << readResult.error << endl;
    exit(1);
  }
  vector<KVPair> dbRecord;
  parseStringToVector(contents, dbRecord);
  for (auto &dbPair : dbRecord)
  {
    for (auto &valPair : values)
    {
      if (dbPair.first == valPair.first)
      {
        dbPair.second = valPair.second;
        break;
      }
    }
  }

  auto dbRecordStr = toString(dbRecord);
  // NOTE: no concurrency control, there will be lost-update anomalies.
  auto result = tree.write(path, dbRecordStr);
  if (result.status != Status::OK)
  {
    cerr << "write: " << result.error << endl;
    exit(1);
  }

  return DB::kOK;
}

int LogCabinDB::Insert(const std::string &table, const std::string &key,
                       std::vector<KVPair> &values)
{
  auto valuesStr = toString(values);
  cout << "INSERT table: " << table << ", key " << key << ", values " << valuesStr << endl;
  auto tree = cluster->getTree();
  tree.setTimeout(TIMEOUT_NANOS);
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
