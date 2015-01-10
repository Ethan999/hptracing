/* 
* @Author: BlahGeek
* @Date:   2015-01-10
* @Last Modified by:   BlahGeek
* @Last Modified time: 2015-01-10
*/

#ifndef __hp_cl_program_h__
#define __hp_cl_program_h__ value

#include "./unit/types.h"

namespace hp {

class CLProgram {
public:
    cl_device_id device_id;
    cl_context context;
    cl_command_queue commands;
    cl_program program;

    cl_kernel getKernel(const char * kernel_name);
    cl_mem createBuffer(cl_mem_flags flags, size_t len, void * host_p);
    CLProgram();
    ~CLProgram();

};

}

#endif