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
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

namespace rjson = rapidjson;

std::pair<std::string, long> ParseQuery(const std::string &line) {
  rjson::Document doc;
  doc.Parse(line.c_str());
  std::string sql = doc["sql"].GetString();
  long timestamp = static_cast<long>(doc["sql"].GetDouble());
  return std::make_pair(std::move(sql), timestamp);
}

std::vector<std::pair<std::string, long>> LoadWorkloadTrace(const std::string &file) {
  std::vector<std::pair<std::string, long>> res;
  long prev_timestamp = 0;
  std::cout << "Loading workload trace..." << std::endl;
  std::ifstream infile(file);
  if (infile.fail()) {
    return std::move(res);
  }
  std::string line;
  long think_time = 0;
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
  }
  std::cout << "Workload trace loaded" << std::endl;
  return std::move(res);
}

std::unique_ptr<sql::Connection> ConnectToDb(const std::string &server) {
  auto driver = sql::mysql::get_mysql_driver_instance();
  sql::ConnectOptionsMap connection_properties;
  connection_properties["hostName"] = server;
  connection_properties["userName"] = "root";
  connection_properties["password"] = "";
  connection_properties["schema"] = "lobsters";
  connection_properties["port"] = 4243;
  connection_properties["OPT_RECONNECT"] = true;
  std::cout << "Connecting to database..." << std::endl;
  sql::Connection *conn = nullptr;
  try {
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
  }
  return std::unique_ptr<sql::Connection>(conn);
}

std::vector<long> Replay(sql::Connection *conn,
  std::vector<std::pair<std::string, long>> &&trace) {
  std::cout << "Replay starts" << std::endl;
  auto total = trace.size();
  std::vector<long> latencies;
  std::chrono::high_resolution_clock::time_point trx_start;
  std::unique_ptr<sql::Statement> stmt(conn->createStatement());
  try {
    for (size_t i = 0; i < total; i++) {
      auto &query = trace[i];
      std::cout << "\rReplay of " << i + 1 << "/" << total;
      if (query.second > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(query.second));
      }
      if (query.first == "BEGIN") {
        trx_start = std::chrono::high_resolution_clock::now();
      } else if (query.first == "COMMIT") {
        conn->commit();
        auto trx_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(trx_end - trx_start);
        latencies.push_back(duration.count());
      } else {
        stmt->execute(query.first);
      }
    }
  } catch (sql::SQLException &e) {
    std::cout << "# ERR: " << e.what();
    std::cout << " (MySQL error code: " << e.getErrorCode();
    std::cout << ", SQLState: " << e.getSQLState() << " )" << std::endl;
  }
  std::cout << "\nReplay finished." << std::endl;
  return std::move(latencies);
}

double Mean(std::vector<long> &latencies) {
  if (latencies.size() == 0) {
    return 0;
  }
  double mean = 0;
  double i = 1;
  for (auto latency : latencies) {
    mean == (latency - mean) / i;
    i++;
  }
  return mean;
}

void DumpLatencies(std::vector<long> &&latencies, const std::string &file) {
  std::ofstream latency_file(file);
  for (auto latency : latencies) {
    latency_file << latency << std::endl;
  }
  latency_file.close();
  std::cout << "Mean latency is " << Mean(latencies) << "us" << std::endl;
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
  auto conn = std::move(::ConnectToDb(server));
  auto trace = ::LoadWorkloadTrace(workload_file);
  auto latencies = ::Replay(conn.get(), std::move(trace));
  ::DumpLatencies(std::move(latencies), latency_file);
  return 0;
}
