#include <stdio.h>
#include "string_map.h"
int main(int argc, char *argv[])
{
   Map* tmap = StringMap_create();
   Map_addEntry(tmap,"test1","bbbbb");
   printf("\033[32m[%s %d]%s\033[0m\n", __func__, __LINE__, Map_getEntry(tmap, "test1"));
    return 0;
}
