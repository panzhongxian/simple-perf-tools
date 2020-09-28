/* Copyright [2020] <panzhongxian0532@gmail.com> */
#include <stdio.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

class ProcCpuInfo;

// Global variables
int hertz;
std::vector<ProcCpuInfo> cpu_info_list;

// Class defination
class ProcCpuInfo {
 public:
  explicit ProcCpuInfo(uint64_t pid) : pid_(pid), last_uptime_(0) {
    UpdateProcCpuInfo();
  }

  std::tuple<double, double> UpdateProcCpuInfo() {
    double uptime = get_uptime();
    std::string stat_file_name = "/proc/" + std::to_string(pid_) + "/stat";
    std::ifstream f(stat_file_name.c_str());
    std::string field;
    int field_index = 1;
    uint64_t utime, stime, cutime, cstime;

    while (f >> field) {
      switch (field_index++) {
        case 14:
          utime = static_cast<uint64_t>(atoll(field.c_str()));
          break;
        case 15:
          stime = static_cast<uint64_t>(atoll(field.c_str()));
          break;
        case 16:
          cutime = static_cast<uint64_t>(atoll(field.c_str()));
          break;
        case 17:
          cstime = static_cast<uint64_t>(atoll(field.c_str()));
          break;
      }
      if (field_index > 20) {
        break;
      }
    }

    // cpu_time = utime + stime + cutime + cstime;

    if (!last_uptime_) {
      last_uptime_ = uptime;
      last_utime_ = utime;
      last_stime_ = stime;
      return std::make_tuple(0, 0);
    }

    double seconds = uptime - last_uptime_;
    total_cpu_usage_ = 100.0 * (utime + stime - last_utime_ - last_stime_) /
                       hertz / (uptime - last_uptime_);
    sys_cpu_usage_ =
        100.0 * (stime - last_stime_) / hertz / (uptime - last_uptime_);
    last_uptime_ = uptime;
    last_utime_ = utime;
    last_stime_ = stime;
    return std::make_tuple(total_cpu_usage_,
                           total_cpu_usage_ * stime / (stime + utime));
  }

  int get_pid() { return pid_; }

 private:
  uint64_t pid_;
  double last_uptime_;
  uint64_t last_utime_;
  uint64_t last_stime_;
  int total_cpu_usage_;
  int sys_cpu_usage_;

  static double get_uptime() {
    /*
      // too slow in docker
      ifstream f("/proc/uptime");

      double a, b;
      std::string line;
      getline(f, line);
      sscanf(line.c_str(), "%lf %lf", &a, &b);
      return a;
    */
    static auto start_time = std::chrono::system_clock::now();
    return static_cast<std::chrono::duration<double>>(
               std::chrono::system_clock::now() - start_time)
        .count();
  }
};

std::string GetBashCmdResult(const std::string& cmd) {
  std::array<char, 128> buffer;
  std::string result;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"),
                                                pclose);
  if (!pipe) {
    throw std::runtime_error("popen() failed!");
  }
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
    result += buffer.data();
  }
  return result;
}

std::vector<int> GetAllProcId(const std::string& proc_name) {
  std::vector<int> ret;
  std::string cmd = "/usr/bin/ps -ef | grep " + proc_name      //
                    + "| grep -v " + std::to_string(getpid())  //
                    + "| grep -v grep | awk '{print $2}'";
  std::stringstream ss;
  ss << GetBashCmdResult(cmd);
  int pid;
  while (ss >> pid) {
    ret.push_back(pid);
  }
  return ret;
}

int main(int argc, char* argv[]) {
  std::string proc_name;
  int gcore_pid = 0;
  int core_cpu_up_threshold = 15;

  if (argc > 1) {
    proc_name = argv[1];
  } else {
    std::cerr << "Usage:" << std::endl;
    std::cerr << "    " << argv[0]
              << " proc_name [gcore_pid] [core_cpu_up_threshold]" << std::endl;
    return 1;
  }

  if (argc > 2) {
    gcore_pid = atoi(argv[2]);
  }
  if (argc > 3) {
    core_cpu_up_threshold = atoi(argv[3]);
  }

  auto pid_list = GetAllProcId(proc_name);
  // TODO(panzhongxian): work in docker?
  hertz = sysconf(_SC_CLK_TCK);

  for (auto pid : pid_list) {
    cpu_info_list.push_back(ProcCpuInfo(pid));
  }

  while (true) {
    usleep(500000);

    std::cout << "==================" << std::endl;
    std::time_t cur_time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::cout << "cpu usage at " << ctime(&cur_time);
    for (auto& cpu_info : cpu_info_list) {
      auto cpu_usage = cpu_info.UpdateProcCpuInfo();
      double total_cpu_usage = std::get<0>(cpu_usage);
      double sys_cpu_usage = std::get<1>(cpu_usage);
      std::cout << cpu_info.get_pid() << ": " << total_cpu_usage << ", (sys) "
                << sys_cpu_usage << std::endl;
      if (gcore_pid && cpu_info.get_pid() == gcore_pid &&
          total_cpu_usage >= core_cpu_up_threshold) {
        std::string gcore_cmd =
            "gcore -o `mktemp` " + std::to_string(gcore_pid);
        system(gcore_cmd.c_str());
      }
    }
  }
  return 0;
}
