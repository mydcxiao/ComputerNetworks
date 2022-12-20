
#include <iostream>
using namespace std;
void f(int i, const int j)
// j is treated as a constant in f()
{
i++; // ok
j = 2; // error
}
int main (){
const int max = 10;
max++;
return 0;
}
