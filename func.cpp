#include <func.h>
#include <iostream>

using namespace std;

int global_int = 1024;
const char static_string[] = "read oooooo data";

int func()
{
    cout << "[" << __PRETTY_FUNCTION__ << "]" << __FILE__ << ":" << __LINE__ << endl;
    return 0;
}

void output_static_int()
{
    static int static_int = 0;
    cout << "static_int = " << static_int++ << endl;
}

static int static_func()
{
    cout << "[static_func]" << endl;
    int a[100];
    int x = 30;
    int n=100;
    int left =0;
    int right=n-1;
    while(left<=right)
    {
        int mid =(left+right)/2;
        if(x==a[mid])
        {
            return mid;
        }
        if(x>a[mid])
        {
            left=mid+1;
        }
        else
        {
            right =mid-1;
        }
    }
    return -1;//未找到x
}

void output_info()
{
    //test_on_rodata();
    func();
    static_func();
    output_static_int();
    cout << "global_int = " << global_int << endl;
    cout << "static_string = " << static_string << endl;
    cout << endl;
}
