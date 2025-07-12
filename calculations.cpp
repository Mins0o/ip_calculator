#include "calculations.hpp"  // Must include the header file

#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace ip_calculator {

struct UInt32Range {
  uint32_t start;
  uint32_t end;

  // For sorting
  bool operator<(const UInt32Range& other) const { return start < other.start; }
};

static uint32_t convertToUint32(const std::string& ip) {
  int a, b, c, d;
  if (sscanf(ip.c_str(), "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
    throw std::invalid_argument("Invalid IP address format");
  }
  return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
         (static_cast<uint32_t>(c) << 8) | static_cast<uint32_t>(d);
}

static std::vector<UInt32Range> convertToUInt32Ranges(
    const std::vector<IPRange>& ranges) {
  std::vector<UInt32Range> out;
  out.reserve(ranges.size());
  for (const auto& r : ranges) {
    uint32_t start = r.ip & r.mask;
    uint32_t end = start | ~r.mask;
    out.push_back({start, end});
  }
  return out;
}

static void normalizeRanges(std::vector<UInt32Range>& ranges) {
  if (ranges.empty()) {
    return;
  }
  std::sort(ranges.begin(), ranges.end());
  std::vector<UInt32Range> merged;
  merged.push_back(ranges[0]);
  for (size_t i = 1; i < ranges.size(); ++i) {
    // Check if the current range overlaps with the last merged range,
    // or if it's directly adjacent to it.
    // when the last merged range already extends to the end of the IPv4 space.
    if (ranges[i].start <= merged.back().end ||
        (merged.back().end != 0xFFFFFFFF &&
         ranges[i].start == merged.back().end + 1)) {
      // If there's an overlap or adjacency, extend the last merged range
      // if the current range goes further.
      if (ranges[i].end > merged.back().end) {
        merged.back().end = ranges[i].end;
      }
    } else {
      // If there is a gap, the current range cannot be merged.
      // Add it as a new, separate range.
      merged.push_back(ranges[i]);
    }
  }
  ranges = merged;
}

static std::vector<UInt32Range> subtractRanges(
    const std::vector<UInt32Range>& includes,
    const std::vector<UInt32Range>& excludes) {
  std::vector<UInt32Range> results = includes;

  for (const auto& exc : excludes) {
    for (size_t i = 0; i < results.size();) {
      auto& res = results[i];

      // Case 1: No overlap. The exclusion is completely before or after the
      // current range.
      if (exc.end < res.start || exc.start > res.end) {
        i++;  // This range is unaffected, move to the next.
        continue;
      }

      // Case 2: The exclusion completely covers this range.
      // [ res.start ... res.end ]
      // [ exc.start ......... exc.end ]
      if (exc.start <= res.start && exc.end >= res.end) {
        results.erase(results.begin() + i);  // Erase it.
        continue;  // Check the same index again as elements have shifted.
      }

      // Case 3: The exclusion splits the range into two.
      // [ res.start .... [ exc.start ... exc.end ] .... res.end ]
      if (exc.start > res.start && exc.end < res.end) {
        uint32_t original_end = res.end;
        res.end = exc.start - 1;  // Modify current range to be the left part.
        // Insert the new right part after the current one.
        results.insert(results.begin() + i + 1, {exc.end + 1, original_end});
        i += 2;  // Skip over both of the now-processed ranges.
        continue;
      }

      // Case 4: The exclusion truncates the start of the range.
      // [ exc.start ... [ res.start ... exc.end ] ... res.end ]
      if (exc.start <= res.start) {
        res.start = exc.end + 1;  // Move the start of the range forward.
      } else {
        // Case 5: The exclusion truncates the end of the range.
        // [ res.start ... [ exc.start ... res.end ] ... exc.end ]
        res.end = exc.start - 1;  // Move the end of the range backward.
      }
      i++;  // Move to the next range.
    }
  }
  return results;
}

static std::vector<IPRange> convertRangeToCidrs(const UInt32Range& range,
                                                bool verbose) {
  std::vector<IPRange> result;
  uint32_t current_ip = range.start;

  if (verbose) {
    std::cerr << "Converting range [" << convertToString(range.start) << " - "
              << convertToString(range.end) << "] to CIDRs...\n";
  }

  // Use a check for range.end to prevent overflow with current_ip
  while (current_ip >= range.start && current_ip <= range.end) {
    uint32_t size = 1;
    while (true) {
      // Prevent overflow when calculating next_size
      if (size > 0xFFFFFFFF / 2) break;
      uint32_t next_size = size << 1;
      uint32_t next_mask = ~(next_size - 1);

      // Check for alignment and if the next block would go past the end
      // This check is safe against overflow.
      if ((current_ip & next_mask) != current_ip ||
          current_ip > range.end - (next_size - 1)) {
        break;
      }
      size = next_size;
    }
    int prefix_len = 32 - __builtin_ctz(size);
    uint32_t mask =
        (prefix_len == 32) ? 0xFFFFFFFF : ~((1U << (32 - prefix_len)) - 1);
    if (prefix_len == 0) mask = 0;
    result.push_back({current_ip, mask});

    if (verbose) {
      std::cerr << "  -> Found CIDR: " << convertToString(current_ip) << "/"
                << prefix_len << " (size: " << size << ")\n";
    }

    // Check for end of range BEFORE adding size to prevent overflow.
    if (current_ip == range.end) break;
    if (current_ip > 0xFFFFFFFF - size) break;

    current_ip += size;
  }
  return result;
}

// --- Public Function Definitions ---

IPRange parseRange(const std::string& range) {
  IPRange ipRange;
  size_t slashPos = range.find('/');
  if (slashPos == std::string::npos) {
    throw std::invalid_argument("Invalid range format, expected 'IP/MASK'");
  }
  std::string ipPart = range.substr(0, slashPos);
  std::string maskPart = range.substr(slashPos + 1);
  ipRange.ip = convertToUint32(ipPart);
  int maskBits = std::stoi(maskPart);
  if (maskBits < 0 || maskBits > 32) {
    throw std::invalid_argument("Invalid mask bits, must be between 0 and 32");
  }
  ipRange.mask = (maskBits == 0) ? 0 : (0xFFFFFFFF << (32 - maskBits));
  return ipRange;
}

std::string convertToString(uint32_t ip) {
  return std::to_string((ip >> 24) & 0xFF) + "." +
         std::to_string((ip >> 16) & 0xFF) + "." +
         std::to_string((ip >> 8) & 0xFF) + "." + std::to_string(ip & 0xFF);
}

int convertMaskToPrefixLength(uint32_t mask) {
  return __builtin_popcount(mask);
}

std::vector<IPRange> processRanges(const std::vector<IPRange>& includes,
                                   const std::vector<IPRange>& excludes,
                                   bool verbose) {
  std::vector<UInt32Range> include_ranges;
  if (includes.empty()) {
    include_ranges.push_back({0, 0xFFFFFFFF});
  } else {
    include_ranges = convertToUInt32Ranges(includes);
    normalizeRanges(include_ranges);
  }

  std::vector<UInt32Range> exclude_ranges = convertToUInt32Ranges(excludes);
  normalizeRanges(exclude_ranges);

  if (verbose) {
    std::cerr << "Normalized " << include_ranges.size()
              << " include ranges and " << exclude_ranges.size()
              << " exclude ranges.\n";
  }

  std::vector<UInt32Range> final_ranges =
      subtractRanges(include_ranges, exclude_ranges);

  if (verbose) {
    std::cerr << "Subtraction resulted in " << final_ranges.size()
              << " final ranges to process.\n";
  }

  std::vector<IPRange> result;
  for (const auto& range : final_ranges) {
    auto cidrs = convertRangeToCidrs(range, verbose);
    result.insert(result.end(), cidrs.begin(), cidrs.end());
  }

  return result;
}

}  // namespace ip_calculator