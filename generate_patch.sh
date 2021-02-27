src_file=new_func.cpp
target=./patch.so

g++ -I./ $src_file -o $target -shared -fPIC -rdynamic -std=c++11  -g

pkill -SIGUSR1 main

