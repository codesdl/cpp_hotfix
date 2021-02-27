#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <fstream>
#include "func.h"
#include "load_patch.h"

using namespace std;

static const char* patch_list = "patch_lists.txt";

void load_patch_by_sig(int signum)
{
    string name = "./patch.so";
    do_fix_by_so(name);

    // append this library to the patch_list
    ifstream ifile(patch_list, ios::in);
    if (ifile.is_open())
    {
        string old_patch;
        while (getline(ifile, old_patch))
        {
            if (old_patch == name)
            {
                ifile.close();
                return;
            }
        }
    }

    ofstream ofile(patch_list, ios::out | ios::app);
    ofile << name << endl;
    ofile.close();
}

void reg_signal()
{
    struct sigaction sa_usr;
    memset(&sa_usr, 0, sizeof(sa_usr));
    sa_usr.sa_flags = 0;
    sa_usr.sa_handler = load_patch_by_sig;   //信号处理函数
    sigaction(SIGUSR1, &sa_usr, nullptr);
}

int main()
{
    reg_signal();

    output_info();

    // after init load patch_lists.txt if exists
    load_patch(patch_list);
    // main loop
    while (true)
    {
        usleep(2 * 1000000);
        output_info();
    }
    return 0;
}
