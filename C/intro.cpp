#include <iostream>

#define dsquare(x) x*x

int inline isquare(int x)
{
    return x*x;
}

int main()
{
  std::cout << dsquare(3+3) << std::endl;
  std::cout << isquare(3+3) << std::endl;
  return 0;
}
