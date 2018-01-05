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
const int kMinTime = 1001;
const int kMaxTime = 1100;
const int kAvgTime = 1050;
const int kQueryCount = kMaxTime - kMinTime + 1;
const int kThinkTime = 100;

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

std::string NumberedQuery(size_t index, const std::string &query) {
  char digits[kNumIndexDigits + 1];
  sprintf(digits, "%-10lu", index);
  return std::string(digits, kNumIndexDigits) + query;
}

std::string GetQueryAllEqual(size_t i) {
  auto sleep_time = std::to_string(static_cast<double>(kAvgTime) * 1e-6);
  return "SELECT SLEEP(" + sleep_time + ");";
}

std::string GetQueryBad(size_t i) {
  double sleep_time = 0;
  sleep_time = (i % 2 == 0) ? kMinTime : kAvgTime + i / 2;
  return "SELECT SLEEP(" + std::to_string(sleep_time * 1e-6) + ");";
}

void Replay(const std::string &server,
            std::vector<long> &query_latencies,
            std::vector<long> &trx_latencies) {
  auto conn = std::move(::ConnectToDb(server));
  if (conn.get() == nullptr) {
    exit(EXIT_FAILURE);
  }
  std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  for (size_t i = 0; i < kQueryCount; i++) {
    auto query = GetQueryAllEqual(i);
    std::cout << "\rReplay of " << i + 1 << "/" << kQueryCount << std::flush;
    std::this_thread::sleep_for(std::chrono::microseconds(kThinkTime));
    try {
      auto start = Now();
      stmt->execute(NumberedQuery(i, query));
      query_latencies.push_back(GetDuration(start));
      delete stmt->getResultSet();
    } catch (sql::SQLException &e) {
      std::cout << "\n# ERR: " << e.what();
      std::cout << " (MySQL error code: " << e.getErrorCode();
      std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
    }
  }
  std::cout << "\nReplay finished." << std::endl;
}

void DumpTrxLatencies(std::vector<long> &&latencies, const std::string &file) {
  std::ofstream latency_file("trx_" + file);
  for (auto latency : latencies) {
    latency_file << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency is " << Mean(latencies) << "us out of " << latencies.size() << " transactions" << std::endl;
}

void DumpQueryLatencies(std::vector<long> &&latencies, const std::string &file) {
  std::ofstream latency_file("query_" + file);
  for (auto latency : latencies) {
    latency_file << 0 << ',' << latency << std::endl;
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
  std::vector<long> trx_latencies;
  std::vector<long> query_latencies;
  RestartServers();
  Replay(server, query_latencies, trx_latencies);
  ::DumpTrxLatencies(std::move(trx_latencies), latency_file);
  ::DumpQueryLatencies(std::move(query_latencies), latency_file);

  return 0;
}
