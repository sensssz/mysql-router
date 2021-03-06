#include "query_parser.h"
#include "mysql_connection.h"
#include "mysql_driver.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include "rapidjson/document.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <cstdio>

namespace {

using TimePoint = std::chrono::high_resolution_clock::time_point;
namespace rjson = rapidjson;

const int kNumIndexDigits = 20;

TimePoint Now() {
  return std::chrono::high_resolution_clock::now();
}

long GetDuration(TimePoint &start) {
  auto end = Now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return static_cast<long>(duration.count());
}

double Mean(const std::vector<long> &latencies) {
  double mean = 0;
  double i = 1;
  for (auto latency : latencies) {
    mean += (latency - mean) / i;
    i++;
  }
  return mean;
}

std::pair<std::string, long> ParseQuery(const std::string &line) {
  rjson::Document doc;
  doc.Parse(line.c_str());
  std::string sql = doc["sql"].GetString();
  long timestamp = static_cast<long>(doc["time"].GetDouble());
  return std::make_pair(std::move(sql), timestamp);
}

void LoadWorkloadTrace(
  const std::string &file,
  std::vector<std::string> &queries,
  std::vector<int> &query_ids,
  std::vector<long> &think_times) {
  long prev_timestamp = 0;
  QueryParser parser;
  std::cout << "Loading workload trace..." << std::endl;
  std::ifstream infile(file);
  if (infile.fail()) {
    return;
  }
  std::string line;
  long think_time = 0;
  int num_reads = 0;
  int num_writes = 0;
  std::vector<long> sizes;
  int current_size = 0;
  int num_commits = 0;
  while (!infile.eof()) {
    std::getline(infile, line);
    if (line.length() <= 1) {
      continue;
    }
    auto pair = ParseQuery(line);
    if (prev_timestamp == 0) {
      think_time = 0;
    } else {
      think_time = pair.second - prev_timestamp;
    }
    auto query_id = parser.GetQueryId(pair.first);
    queries.push_back(pair.first);
    query_ids.push_back(query_id);
    think_times.push_back(think_time);
    prev_timestamp = pair.second;
    if (pair.first.find("BEGIN") == 0) {
      current_size = 0;
      think_times.back() = 0;
    } else if (pair.first.find("COMMIT") == 0) {
      sizes.push_back(current_size);
      ++num_commits;
      if (num_commits == 500) {
        break;
      }
    } else if (pair.first.find("SELECT") == 0) {
      num_reads++;
      current_size++;
    } else {
      num_writes++;
      current_size++;
    }
  }
  parser.DumpTemplates("templates");
  std::cout << "Workload trace loaded" << std::endl;
  std::cout << "Average think time is " << Mean(think_times) << "us" << std::endl;
  std::cout << num_reads << " reads and " << num_writes << " writes" << std::endl;
  std::cout << "Average transaction size is " << Mean(sizes) << std::endl;
}

std::set<size_t> LoadWaitQueries(const std::string &filename) {
  std::set<size_t> wait_queries;
  std::ifstream infile(filename);
  if (infile.fail()) {
    std::cerr << "Wait query file not found" << std::endl;
    return std::move(wait_queries);
  }
  size_t query_index;
  std::cout << "Loading wait queries" << std::endl;
  while (!infile.eof()) {
    infile >> query_index;
    wait_queries.insert(query_index);
  }

  return std::move(wait_queries);
}

std::unique_ptr<sql::Connection> ConnectToDb(const std::string &server, const std::string &database) {
  auto driver = sql::mysql::get_mysql_driver_instance();
  std::string url = server + ":4242";
  std::cout << "Connecting to database..." << std::endl;
  sql::Connection *conn = nullptr;
  auto attempts = 0;
  while (conn == nullptr && attempts < 10) {
    try {
      sql::ConnectOptionsMap connection_properties;
      connection_properties["hostName"] = server;
      connection_properties["port"] = 4242;
      connection_properties["userName"] = "root";
      connection_properties["password"] = "";
      connection_properties["schema"] = database;
      connection_properties["CLIENT_MULTI_STATEMENTS"] = true;
      conn = driver->connect(connection_properties);
      if (!conn->isValid()) {
        std::cout << "Connection fails" << std::endl;
        delete conn;
        conn = nullptr;
      } else {
        std::cout << "Connection established" << std::endl;
        conn->setAutoCommit(false);
      }
    } catch (sql::SQLException &e) {
      std::cout << "# ERR: " << e.what();
      std::cout << " (MySQL error code: " << e.getErrorCode();
      std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
      conn = nullptr;
    }
    attempts++;
  }
  return std::unique_ptr<sql::Connection>(conn);
}

// Rewind to start of transaction
size_t Rewind(size_t start,
              const std::vector<std::string> &queries) {
  for (; start > 0; start--) {
    auto &query = queries[start];
    if (query == "BEGIN") {
      return start;
    }
  }
  return 0;
}

std::string NumberedQuery(size_t index, int query_id, const std::string &query) {
  char digits[kNumIndexDigits];
  sprintf(digits, "%-10lu%-9d", index, query_id);
  return std::string(digits, kNumIndexDigits) + query;
}

/*
void WarmUp(sql::Connection *conn,
            const std::vector<std::pair<std::string, long>> &trace) {
  std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  std::cout << "Warm up starts" << std::endl;
  auto total = trace.size();
  for (size_t i = 0; i < trace.size(); i++) {
    std::cout << '\r' << i + 1 << '/' << total << std::flush;
    auto &query = trace[i];
    if (query.first.find("SELECT") != 0) {
      continue;
    }
    stmt->execute(NumberedQuery(i, query.first));
    delete stmt->getResultSet();
  }
  std::cout << std::endl << "Warm up complets" << std::endl;
}
*/

void Replay(int ID, const std::string &server,
            const std::string &database,
            std::set<size_t> wait_queries,
            std::vector<std::pair<int, long>> &query_latencies,
            std::vector<long> &trx_latencies,
            const std::vector<std::string> &queries,
            const std::vector<int> &query_ids,
            const std::vector<long> &think_times) {
  auto conn = std::move(::ConnectToDb(server, database));
  if (conn.get() == nullptr) {
    exit(EXIT_FAILURE);
  }
  auto total = queries.size();
  TimePoint trx_start;
  std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  stmt->execute("ID=" + std::to_string(ID));
  stmt->execute("set autocommit=0");
  for (size_t i = 0; i < total; i++) {
    auto &query = queries[i];
    auto query_id = query_ids[i];
    auto think_time = think_times[i];
    std::cout << "\rReplay of " << i + 1 << "/" << total << std::flush;
    if (query != "BEGIN" && think_time > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(think_time));
      /*
      if (wait_queries.find(i) != wait_queries.end()) {
        std::this_thread::sleep_for(std::chrono::microseconds(8000000));
      } else {
        std::this_thread::sleep_for(std::chrono::microseconds(think_time));
      }
      */
    }
    if (query == "COMMIT") {
      conn->commit();
      trx_latencies.push_back(GetDuration(trx_start));
    } else {
      bool is_begin = query == "BEGIN";
      if (is_begin) {
        trx_start = Now();
      }
      auto start = Now();
      try {
        bool is_select = stmt->execute(NumberedQuery(i, query_id, query));
        if (!is_begin) {
          query_latencies.push_back(std::make_pair(query_id, GetDuration(start)));
        }
        if (is_select) {
          delete stmt->getResultSet();
        }
      } catch (sql::SQLException &e) {
        if (!is_begin) {
          query_latencies.push_back(std::make_pair(query_id, GetDuration(start)));
        }
        std::cout << "\n# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        // Reconnect when lost connection
        if (e.getErrorCode() == 2006 || e.getErrorCode() == 2013 || e.getErrorCode() == 2014 || e.getErrorCode() == 1046) {
          break;
        }
        else if (e.getErrorCode() == 1064) {
          break;
        }
        // Rewind when transaction times out
        else if (e.getErrorCode() == 1205) {
          i = Rewind(i, queries) - 1;
        }
        // Skip for duplicates or syntax error
        else if (e.getErrorCode() != 1062 &&
                 e.getErrorCode() != 1065 &&
                 e.getErrorCode() != 2014) {
          break;
        }
      }
    }
  }
  std::cout << "\nReplay finished." << std::endl;
}

void RestartServers() {
  system("ssh client /users/POTaDOS/SQP/.local/bin/mrstart");
  for (auto i = 1; i <= 2; i++) {
    auto command = "ssh server" + std::to_string(i) + R"( /bin/bash <<EOF
      source .bashrc;
      ~/SQP/.local/mysql/support-files/mysql.server stop;
      echo '' > ~/.local/mysql/mysqld.log;
      rm -rf ~/.local/mysql/data;
      cp -r ~/SQP/lobsters ~/.local/mysql/data;
      sleep 5s;
      ~/SQP/.local/mysql/support-files/mysql.server start;
EOF)";
    system(command.c_str());
  }
}

std::vector<std::pair<int, long>> GetQueryProcessLatencies(const std::vector<int> &query_ids, const std::string &filename, int ID) {
  std::string tmpname = std::tmpnam(nullptr);
  const std::string &command = "scp client:" + filename + std::to_string(ID) + " " + tmpname;
  system(command.c_str());
  std::vector<std::pair<int, long>> query_process_latencies;
  std::ifstream infile(tmpname);
  if (infile.fail()) {
    return std::move(query_process_latencies);
  }
  long latency;
  for (size_t i = 0; i < query_ids.size(); i++) {
    auto query_id = query_ids[i];
    infile >> latency;
    query_process_latencies.push_back(std::make_pair(query_id, latency));
  }
  return std::move(query_process_latencies);
}

std::vector<std::pair<int, long>> GetQueryProcessLatencies(const std::string &filename, int ID) {
  std::string tmpname = std::tmpnam(nullptr);
  const std::string &command = "scp client:" + filename + std::to_string(ID) + " " + tmpname;
  system(command.c_str());
  std::vector<std::pair<int, long>> query_process_latencies;
  std::ifstream infile(tmpname);
  if (infile.fail()) {
    return std::move(query_process_latencies);
  }
  int query_id;
  long latency;
  while (!infile.eof()) {
    infile >> query_id;
    infile >> latency;
    query_process_latencies.push_back(std::make_pair(query_id, latency));
  }
  return std::move(query_process_latencies);
}

void ClientThread(int ID, const std::string &server,
                  const std::string &database,
                  std::mutex &mutex,
                  std::vector<long> &trx_latencies,
                  std::vector<std::pair<int, long>> &e2e_query_latencies,
                  std::vector<std::pair<int, long>> &read_latencies,
                  std::vector<std::pair<int, long>> &write_latencies,
                  std::vector<std::pair<int, long>> &server_query_latencies,
                  const std::vector<std::string> &queries,
                  const std::vector<int> &query_ids,
                  const std::vector<long> &think_times,
                  const std::set<size_t> &wait_queries) {
  std::vector<long> local_trx_latencies;
  std::vector<std::pair<int, long>> local_e2e_query_latencies;
  ::Replay(ID, server, database, wait_queries, local_e2e_query_latencies,
           local_trx_latencies, queries, query_ids, think_times);
  auto local_server_query_latencies = ::GetQueryProcessLatencies("query_process", ID);
  auto local_read_latencies = ::GetQueryProcessLatencies("read_process", ID);
  auto local_write_latencies = ::GetQueryProcessLatencies("write_process", ID);
  {
    std::unique_lock<std::mutex> l(mutex);
    trx_latencies.insert(trx_latencies.end(), local_trx_latencies.begin(), local_trx_latencies.end());
    e2e_query_latencies.insert(e2e_query_latencies.end(), local_e2e_query_latencies.begin(), local_e2e_query_latencies.end());
    read_latencies.insert(read_latencies.end(), local_read_latencies.begin(), local_read_latencies.end());
    write_latencies.insert(write_latencies.end(), local_write_latencies.begin(), local_write_latencies.end());
    server_query_latencies.insert(server_query_latencies.end(),
                                   local_server_query_latencies.begin(),
                                   local_server_query_latencies.end());
  }
}

void DumpTrxLatencies(std::vector<long> &latencies, const std::string &postfix) {
  std::ofstream latency_file("latencies/trx_latencies_" + postfix);
  for (auto latency : latencies) {
    latency_file << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency is " << Mean(latencies)
            << "us out of " << latencies.size()
            << " transactions" << std::endl;
}

void DumpQueryLatencies(const std::vector<int> &query_ids,
                        const std::vector<long> &latencies,
                        const std::string &name,
                        const std::string &postfix) {
  std::ofstream latency_file("latencies/" + name + "_latencies_" + postfix);
  for (size_t i = 0; i < query_ids.size(); i++) {
    auto query_id = query_ids[i];
    auto latency = latencies[i];
    latency_file << query_id << ',' << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency of " << name << " is "
            << Mean(latencies) << "us out of "
            << latencies.size() << " queries" << std::endl;
}

void DumpQueryLatencies(const std::vector<std::pair<int, long>> &query_process_latencies,
                        const std::string &filename, const std::string &postfix) {
  auto local_filename = "latencies/" + filename + "_" + postfix;
  std::ofstream outfile(local_filename);
  for (auto &latency : query_process_latencies) {
    outfile << latency.first << ',' << latency.second << std::endl;
  }
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 6) {
    std::cout << "Usage: " << argv[0] << " [server] [database] [num_clients] [workload_trace] [postfix]" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::string server(argv[1]);
  std::string database(argv[2]);
  int num_clients = atoi(argv[3]);
  std::string workload_file(argv[4]);
  std::string postfix(argv[5]);

  // auto conn = std::move(::ConnectToDb(server, database));
  // if (conn.get() == nullptr) {
  //   exit(EXIT_FAILURE);
  // }
  // std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  // std::string line;
  // while (true) {
  //   std::getline(std::cin, line);
  //   if (line.find("quit") == 0) {
  //     break;
  //   }
  //   stmt->execute(line);
  //   while (stmt->getMoreResults()) {
  //     std::unique_ptr<sql::ResultSet> res(stmt->getResultSet());
  //   }
  // }

  // return 1;

  std::vector<std::string> queries;
  std::vector<int> query_ids;
  std::vector<long> think_times;
  ::LoadWorkloadTrace(workload_file, queries, query_ids, think_times);
  std::set<size_t> wait_queries = LoadWaitQueries("wait_queries");
  std::vector<long> trx_latencies;
  std::vector<std::pair<int, long>> e2e_query_latencies;
  std::vector<std::pair<int, long>> server_query_latencies;
  std::vector<std::pair<int, long>> read_latencies;
  std::vector<std::pair<int, long>> write_latencies;
  std::vector<std::thread> clients;
  RestartServers();
  std::mutex mutex;
  for (int i = 0; i < num_clients; i++) {
    std::thread client([&, i]() {
      std::cout << "Spawning client " << i << std::endl;
      ::ClientThread(i, server, database, mutex, trx_latencies,
                     e2e_query_latencies, read_latencies, write_latencies,
                     server_query_latencies, queries, query_ids,
                     think_times, wait_queries);
    });
    clients.push_back(std::move(client));
  }
  for (auto &client : clients) {
    client.join();
  }
  ::DumpTrxLatencies(trx_latencies, postfix);
  ::DumpQueryLatencies(e2e_query_latencies, "e2e_query", postfix);
  ::DumpQueryLatencies(read_latencies, "read", postfix);
  ::DumpQueryLatencies(write_latencies, "write", postfix);
  ::DumpQueryLatencies(server_query_latencies, "server_query", postfix);

  return 0;
}
