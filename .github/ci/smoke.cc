#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <vector>

int main()
{
  std::vector<int> values{4, 1, 3, 2};
  std::sort(values.begin(), values.end());
  if (values.front() != 1 || std::accumulate(values.begin(), values.end(), 0) != 10)
    return 1;
  try {
    throw std::runtime_error("exception runtime check");
  } catch (const std::runtime_error &) {
    return 0;
  }
}
