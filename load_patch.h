#pragma once

#include <string>

enum patch_fix_type_e
{
    patch_fix_func_by_addr,
    patch_fix_func_by_name,
    patch_fix_var_by_addr,
    patch_fix_var_by_name,
    patch_fix_end,
};

struct patch_fix_item_t
{
    patch_fix_type_e type;
    char old_val[1024];
    char new_name[1024];
    size_t size;
};

int load_patch(const std::string &file_name);

void do_fix_by_so(const std::string &patch_name);
