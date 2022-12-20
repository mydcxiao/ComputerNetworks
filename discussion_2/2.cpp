
#include <iostream>
using namespace std;
int* f (int){
    cout<<"we are inside the function f\n";
    return (int*)85439245;
};
int main (){
   bool flag1, flag2;
   flag1 = false;
   flag2 =  f(5);
   cout << flag2<<"\n I added a defination of f()"; 
   return 0;
}
