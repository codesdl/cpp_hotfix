#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <dlfcn.h>
#include <sys/mman.h>
#include "load_patch.h"
#include <unistd.h>
#include <cstring>
#include <cstdio>

static const std::string __items_to_be_fixed__ = "__g_items_to_be_fixed__";

int get_program_name(char *buff, size_t size)
{
    char path[1024];
	int ret = readlink("/proc/self/exe", path, sizeof(path) - 1);
	if(ret == -1)
	{
		printf("---- get exec name fail!!\n");
		return -1;
	}
	buff[ret]= '\0';
 
	char *p = strrchr(path, '/');
	strncpy(buff, p + 1, size - 1);
	return 0;
}

void split_string(const std::string& s, std::vector<std::string>& v, const std::string& c)
{
    std::string::size_type pos1, pos2;
    pos2 = s.find(c);
    pos1 = 0;
    while(std::string::npos != pos2)
    {
        v.push_back(s.substr(pos1, pos2-pos1));
        pos1 = pos2 + c.size();
        pos2 = s.find(c, pos1);
    }
    if(pos1 != s.length())
    {
        v.push_back(s.substr(pos1));
    }
}

int parse_page_permission(void *page_addr)
{
    int perm = PROT_NONE;

    char prg_name[1024];
    int ret = get_program_name(prg_name, sizeof(prg_name));
    if (ret != 0)
    {
        return perm;
    }
    // search mapped pages used by self
    char buff[2048] = {'\0'};
    // snprintf(buff, sizeof(buff), "cat /proc/$PPID/maps | grep %s", prg_name);
    snprintf(buff, sizeof(buff), "cat /proc/$PPID/maps");
    FILE *maps = popen(buff, "r");
    if (maps == nullptr)
    {
        return perm;
    }

    char result[1024]={'\0'};
    while (fgets(result, sizeof(result), maps))
    {
        std::vector<std::string> columns;
        split_string(std::string(result), columns, " ");
        // output is like '00400000-00404000 r-xp' and only first 2 columns are needed

        std::vector<std::string> addrs;
        split_string(columns[0], addrs, "-");
        
        uint64_t addr1 = stoull(addrs[0], nullptr, 16);
        uint64_t addr2 = stoull(addrs[1], nullptr, 16);
        uint64_t addr = uint64_t(page_addr);

        if (!(addr1 <= addr && addr < addr2))
        {
            continue;
        }

        const std::string &perm_str = columns[1];

        if (perm_str[0] == 'r') perm |= PROT_READ;
        if (perm_str[1] == 'w') perm |= PROT_WRITE;
        if (perm_str[2] == 'x') perm |= PROT_EXEC;
        break;
    }

    pclose(maps);
    return perm;
}

int fix_func(void *old_func, void *new_func)
{
    char prefix[] = {'\x48', '\xb8'};   // MOV new_addr, %rax
    char postfix[] = {'\xff', '\xe0'};  // JMP *%rax

    // 计算所需要的的页面数 : mprotect要求地址与页边界对齐，检查要修改的地址空间是否跨越了页边界
    size_t page_size = getpagesize();
    const size_t ins_len = sizeof(prefix) + sizeof(void *) + sizeof(postfix);   // 要修改的指令长度
    char *align_point = (char*)((uint64_t)old_func & ~(page_size - 1));     // 与页边界对齐的地址
    char *align_point2 = (char *)(((uint64_t)old_func + ins_len) & ~(page_size - 1));
    size_t page_cnt = (align_point == align_point2 ? 1 : 2);    // 需要修改的页面数

    if (mprotect(align_point, page_cnt * page_size, PROT_READ | PROT_WRITE | PROT_EXEC))
    {
        return -1;
    }

    memcpy(old_func, prefix, sizeof(prefix));
    memcpy((char *)old_func + sizeof(prefix), &new_func, sizeof(void *));
    memcpy((char *)old_func + sizeof(prefix) + sizeof(void *), postfix, sizeof(postfix));

    if (mprotect(align_point, page_cnt * page_size, PROT_READ | PROT_EXEC))
    {
        return -2;
    }

    return 0;
}

int fix_var(void *old_var, const void *new_var, size_t size)
{
    // 计算所需要的的页面数 : mprotect要求地址与页边界对齐，检查要修改的地址空间是否跨越了页边界
    size_t page_size = getpagesize();
    char *align_point = (char*)((uint64_t)old_var & ~(page_size - 1));     // 与页边界对齐的地址
    char *align_point2 = (char *)(((uint64_t)old_var + size) & ~(page_size - 1));
    size_t page_cnt = (align_point == align_point2 ? 1 : 2);    // 需要修改的页面数

    int permission = parse_page_permission(align_point);
    if (permission == PROT_NONE)
    {
        return -1;
    }

    if (!(permission & PROT_WRITE) && mprotect(align_point, page_cnt * page_size, permission | PROT_WRITE))
    {
        return -2;
    }

    memcpy(old_var, new_var, size);

    if (mprotect(align_point, page_cnt * page_size, permission))
    {
        return -3;
    }

    return 0;
}

void* search_symbol_by_name(void *handler, const std::string &name)
{
    return dlsym(handler, name.c_str());
}

void* convert_addr_by_str(const std::string &addr_str)
{
    uint64_t addr = std::stoull(addr_str, nullptr, 16);
    return (void*)addr;
}

void do_fix_by_so(const std::string &patch_name)
{
    std::cout << "starting fix by library " << patch_name << std::endl;
    void *handler = dlopen(patch_name.c_str(), RTLD_NOW);
    if (handler == nullptr)
    {
        std::cout << "library " << patch_name << " can not be loaded for " << dlerror() << std::endl;
        return;
    }
    void *fix_items_sym = dlsym(handler, __items_to_be_fixed__.c_str());
    if (fix_items_sym == nullptr)
    {
        std::cout << __items_to_be_fixed__ << " in " << patch_name << " not found for " << dlerror() << std::endl;
        return;
    }

    for (patch_fix_item_t *fix_item = (patch_fix_item_t *)fix_items_sym; fix_item; ++fix_item)
    {
        void *new_addr = search_symbol_by_name(handler, fix_item->new_name);
        switch (fix_item->type)
        {
        case patch_fix_func_by_addr:
        {
            if (new_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->new_name << " in " << patch_name << std::endl;
                continue;
            }
            void *old_addr = convert_addr_by_str(fix_item->old_val);
            if (old_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->old_val << std::endl;
                continue;
            }
            fix_func(old_addr, new_addr);
        }
        break;
        case patch_fix_func_by_name:
        {
            if (new_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->new_name << " in " << patch_name << std::endl;
                continue;
            }
            void *self_handler = dlopen(nullptr, RTLD_NOW);
            if (self_handler == nullptr)
            {
                std::cout << "cannot load self handler for " << dlerror() << std::endl;
                continue;
            }
            void *old_addr = search_symbol_by_name(self_handler, fix_item->old_val);
            if (old_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->old_val << std::endl;
                continue;
            }
            fix_func(old_addr, new_addr);
        }
        break;
        case patch_fix_var_by_addr:
        {
            void *old_addr = convert_addr_by_str(fix_item->old_val);
            if (old_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->old_val << std::endl;
                continue;
            }
            fix_var(old_addr, new_addr, fix_item->size);
        }
        break;
        case patch_fix_var_by_name:
        {
            void *self_handler = dlopen(nullptr, RTLD_NOW);
            if (self_handler == nullptr)
            {
                std::cout << "cannot load self handler for " << dlerror() << std::endl;
                continue;
            }
            void *old_addr = search_symbol_by_name(self_handler, fix_item->old_val);
            if (old_addr == nullptr)
            {
                std::cout << "cannot find " << fix_item->old_val << std::endl;
                continue;
            }
            fix_var(old_addr, new_addr, fix_item->size);
        }
        break;
        case patch_fix_end:
        {
            std::cout << "load library " << patch_name << " done" << std::endl;
            return;
        }
        break;
        }
    }
}

int load_patch(const std::string &file_name)
{
    std::ifstream ifile;
    ifile.open(file_name, std::ios::in);
    if (ifile.is_open() == false)
    {
        return 0;
    }

    std::string patch_name;
    while (getline(ifile, patch_name))
    {
        do_fix_by_so(patch_name);
    }
    ifile.close();

    return 0;
}
