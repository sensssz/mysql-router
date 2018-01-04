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
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using TimePoint = std::chrono::high_resolution_clock::time_point;
namespace rjson = rapidjson;

const int kNumIndexDigits = 10;

TimePoint Now() {
  return std::chrono::high_resolution_clock::now();
}

long GetDuration(TimePoint &start) {
  auto end = Now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return static_cast<long>(duration.count());
}

double Mean(std::vector<long> &latencies) {
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

std::vector<std::pair<std::string, long>> LoadWorkloadTrace(
  const std::string &file, std::vector<int> &query_ids) {
  std::vector<std::pair<std::string, long>> res;
  long prev_timestamp = 0;
  QueryParser parser;
  std::cout << "Loading workload trace..." << std::endl;
  std::ifstream infile(file);
  if (infile.fail()) {
    return std::move(res);
  }
  std::string line;
  long think_time = 0;
  int num_reads = 0;
  int num_writes = 0;
  std::vector<long> times;
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
    res.push_back(std::make_pair(pair.first, think_time));
    prev_timestamp = pair.second;
    if (pair.first.find("BEGIN") == 0) {
      current_size = 0;
    } else if (pair.first.find("COMMIT") == 0) {
      sizes.push_back(current_size);
      ++num_commits;
      if (num_commits == 500) {
        break;
      }
    } else if (pair.first.find("SELECT") == 0) {
      num_reads++;
      current_size++;
      times.push_back(think_time);
      query_ids.push_back(parser.GetQueryId(pair.first));
    } else {
      num_writes++;
      current_size++;
      times.push_back(think_time);
      res.pop_back();
    }
  }
  parser.DumpTemplates("templates");
  std::cout << "Workload trace loaded" << std::endl;
  std::cout << "Average think time is " << Mean(times) << "us" << std::endl;
  std::cout << num_reads << " reads and " << num_writes << " writes" << std::endl;
  std::cout << "Average transaction size is " << Mean(sizes) << std::endl;
  return std::move(res);
}

std::unique_ptr<sql::Connection> ConnectToDb(const std::string &server) {
  auto driver = sql::mysql::get_mysql_driver_instance();
  std::string url = server + ":4243";
  std::cout << "Connecting to database..." << std::endl;
  sql::Connection *conn = nullptr;
  auto attempts = 0;
  while (conn == nullptr && attempts < 10) {
    try {
      conn = driver->connect(url, "root", "");
      if (!conn->isValid()) {
        std::cout << "Connection fails" << std::endl;
        delete conn;
        conn = nullptr;
      } else {
        std::cout << "Connection established" << std::endl;
        conn->setSchema("lobsters");
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
              const std::vector<std::pair<std::string, long>> &trace) {
  for (; start > 0; start--) {
    auto &query = trace[start];
    if (query.first == "BEGIN") {
      return start;
    }
  }
  return 0;
}

std::string NumberedQuery(size_t index, const std::string &query) {
  char digits[kNumIndexDigits + 1];
  sprintf(digits, "%-10lu", index);
  return std::string(digits, kNumIndexDigits) + query;
}

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

size_t Replay(const std::string &server,
              size_t start,
              std::vector<long> &query_latencies,
              std::vector<long> &trx_latencies,
              std::set<size_t> &skipped_queries,
              const std::vector<std::pair<std::string, long>> &trace) {
  auto conn = std::move(::ConnectToDb(server));
  if (conn.get() == nullptr) {
    exit(EXIT_FAILURE);
  }
  WarmUp(conn.get(), trace);
  start = Rewind(start, trace);
  auto total = trace.size();
  TimePoint trx_start;
  std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  stmt->execute("set autocommit=0");
  for (size_t i = start; i < total; i++) {
    auto &query = trace[i];
    std::cout << "\rReplay of " << i + 1 << "/" << total << std::flush;
    if (query.first != "BEGIN" && query.second > 0) {
      std::this_thread::sleep_for(std::chrono::microseconds(query.second));
    }
    if (skipped_queries.find(i) != skipped_queries.end()) {
      continue;
    }
    if (query.first == "COMMIT") {
      conn->commit();
      trx_latencies.push_back(GetDuration(trx_start));
    } else {
      bool is_begin = query.first == "BEGIN";
      if (is_begin) {
        trx_start = Now();
      }
      try {
        auto start = Now();
        bool is_select = stmt->execute(NumberedQuery(i, query.first));
        if (!is_begin) {
          query_latencies.push_back(GetDuration(start));
        }
        if (is_select) {
          delete stmt->getResultSet();
        }
      } catch (sql::SQLException &e) {
        std::cout << "\n# ERR: " << e.what();
        std::cout << " (MySQL error code: " << e.getErrorCode();
        std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
        // Reconnect when lost connection
        if (e.getErrorCode() == 2006 || e.getErrorCode() == 2013 || e.getErrorCode() == 2014 || e.getErrorCode() == 1046) {
          return i;
        }
        else if (e.getErrorCode() == 1064) {
          skipped_queries.insert(i);
          return i;
        }
        // Rewind when transaction times out
        else if (e.getErrorCode() == 1205) {
          i = Rewind(i, trace) - 1;
        }
        // Skip for duplicates or syntax error
        else if (e.getErrorCode() != 1062 &&
                 e.getErrorCode() != 1065 &&
                 e.getErrorCode() != 2014) {
          return total;
        }
      }
    }
  }
  std::cout << "\nReplay finished." << std::endl;
  return total;

}

void DumpTrxLatencies(std::vector<long> &&latencies, const std::string &file) {
  std::ofstream latency_file("trx_" + file);
  for (auto latency : latencies) {
    latency_file << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency is " << Mean(latencies) << "us out of " << latencies.size() << " transactions" << std::endl;
}

void DumpQueryLatencies(std::vector<int> &&query_ids, std::vector<long> &&latencies, const std::string &file) {
  std::ofstream latency_file("query_" + file);
  for (size_t i = 0; i < query_ids.size(); i++) {
    auto query_id = query_ids[i];
    auto latency = latencies[i];
    latency_file << query_id << ',' << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency is " << Mean(latencies) << "us out of " << latencies.size() << " queries" << std::endl;
}

void RestartServers() {
  system("/users/POTaDOS/SQP/.local/bin/mrstart");
  for (auto i = 1; i <= 2; i++) {
    auto command = "ssh server" + std::to_string(i) + R"( /bin/bash <<EOF
      source .bashrc;
      ~/SQP/.local/mysql/support-files/mysql.server stop;
      echo '' > ~/SQP/.local/mysql/mysqld.log;
      rm -rf ~/.local/mysql/data;
      cp -r ~/SQP/lobsters ~/.local/mysql/data;
      sleep 5s;
      ~/SQP/.local/mysql/support-files/mysql.server start;
EOF)";
    system(command.c_str());
  }
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cout << "Usage: " << argv[0] << " [server] [workload_trace] [latency_file]" << std::endl;
    exit(EXIT_FAILURE);
  }
  std::string server(argv[1]);
  std::string workload_file(argv[2]);
  std::string latency_file(argv[3]);
  std::vector<int> query_ids;
  auto trace = ::LoadWorkloadTrace(workload_file, query_ids);
  size_t start = 0;
  std::vector<long> trx_latencies;
  std::vector<long> query_latencies;
  std::set<size_t> skipped_queries;
  while(start != trace.size()) {
    RestartServers();
    start = Replay(server, start, query_latencies, trx_latencies, skipped_queries, trace);
  }
  ::DumpTrxLatencies(std::move(trx_latencies), latency_file);
  ::DumpQueryLatencies(std::move(query_ids), std::move(query_latencies), latency_file);

  return 0;
}
