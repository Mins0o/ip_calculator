#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ip_calculator {

// Public interface struct used by main()
struct IPRange {
  uint32_t ip;
  uint32_t mask;
};

// Parses a CIDR string (e.g., "192.168.1.0/24") into an IPRange.
IPRange parseRange(const std::string& range);

// The main processing function.
std::vector<IPRange> processRanges(const std::vector<IPRange>& includes,
                                   const std::vector<IPRange>& excludes,
                                   bool verbose = false);

// Converts a 32-bit integer into a dotted-decimal IP string.
std::string convertToString(uint32_t ip);

// Converts a subnet mask into a prefix length (0-32).
int convertMaskToPrefixLength(uint32_t mask);

}  // namespace ip_calculator