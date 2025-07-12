#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "calculations.hpp"

int main(int argc, char* argv[]) {
  // option -i for include, if omited it is all ranges (0.0.0.0/0)
  // option -e for exclude, if omited, nothing is excluded
  // these ranges can be appended
  // ip_calculator -i 128.0.0/16 -i 192.168.0/24 -e 10.0.0/8
  // this will give a codomain of the includes and excludes the range declared
  // with -e
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0]
              << " -i <include_range> -e <exclude_range>\n";
    return 1;
  }
  // parse command line arguments
  // store the includes and excludes in a vector of pairs
  std::vector<ip_calculator::IPRange> includes;
  std::vector<ip_calculator::IPRange> excludes;
  bool verbose = false;
  bool prefix_length_output = false;
  bool single_line_output = false;
  std::string delimiter = ", ";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-i" && i + 1 < argc) {
      // parse include range
      std::string range = argv[++i];
      auto [ip, mask] = ip_calculator::parseRange(range);
      includes.push_back({ip, mask});
    } else if (arg == "-e" && i + 1 < argc) {
      // parse exclude range
      std::string range = argv[++i];
      auto [ip, mask] = ip_calculator::parseRange(range);
      excludes.push_back({ip, mask});
    } else if (arg == "-v" || arg == "--verbose") {
      verbose = true;
    } else if (arg == "--prefix-length") {
      prefix_length_output = true;
    } else if (arg == "--delimiter" && i + 1 < argc) {
      single_line_output = true;
      delimiter = argv[++i];
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return 1;
    }
  }
  // process the ranges
  std::vector<ip_calculator::IPRange> result =
      ip_calculator::processRanges(includes, excludes, verbose);
  // print the result
  if (single_line_output) {
    for (size_t i = 0; i < result.size(); ++i) {
      const auto& range = result[i];
      int prefix_len = ip_calculator::convertMaskToPrefixLength(range.mask);
      std::cout << ip_calculator::convertToString(range.ip) << "/"
                << prefix_len;
      if (i < result.size() - 1) {
        std::cout << delimiter;
      }
    }
    std::cout << "\n";
  } else {
    for (const auto& range : result) {
      if (prefix_length_output) {
        int prefix_len = ip_calculator::convertMaskToPrefixLength(range.mask);
        std::cout << ip_calculator::convertToString(range.ip) << "/"
                  << prefix_len << "\n";
      } else {
        std::cout << "IP: " << ip_calculator::convertToString(range.ip)
                  << ", Mask: " << ip_calculator::convertToString(range.mask)
                  << "\n";
      }
    }
  }
  return 0;
}