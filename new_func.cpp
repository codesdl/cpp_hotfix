#include "load_patch.h"
#include <iostream>

using namespace std;

int replaced_func()
{
    cout << "[" << __FUNCTION__ << "] " << __FILE__ << ":" << __LINE__ << endl;
    return 0;
}

int replaced_static_func()
{
    cout << "[replaced_static_func]" << endl;
    return 0;
}

int new_static_int = 77777;
int new_global_int = 111111111;
char ro_data[100] = "other data";

patch_fix_item_t __g_items_to_be_fixed__[] = 
{
    {patch_fix_var_by_addr, "0000000000608358", "new_static_int", sizeof(new_static_int)},  // static_int
    {patch_fix_func_by_addr, "0000000000403edb", "_Z20replaced_static_funcv", 10}, // static_func
    {patch_fix_var_by_addr, "0000000000405e90", "ro_data", 11},  // static_string
    {patch_fix_func_by_name, "_Z4funcv", "_Z13replaced_funcv", 0},    // func
    {patch_fix_var_by_name, "global_int", "new_global_int", sizeof(int)},  // global int
    {patch_fix_end, "", "", 0},
};
