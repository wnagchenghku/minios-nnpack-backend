float bias_1_264993A3[64] = {};
float kernelData_1_264993A3[23232] = {0};
float bias_2_264993A3[192] = {};
float kernelData_2_264993A3[307200] = {0};
float bias_3_264993A3[384] = {};
float kernelData_3_264993A3[663552] = {0};
float bias_4_264993A3[256] = {};
float kernelData_4_264993A3[884736] = {0};
float bias_5_264993A3[256] = {};
float kernelData_5_264993A3[589824] = {0};
float kernelData_6_264993A3[37748736] = {0};
float bias_6_264993A3[4096] = {0};

float kernelData_7_264993A3[16777216] = {0};
float bias_7_264993A3[4096] = {0};

float kernelData_8_264993A3[4096000] = {0};
float bias_8_264993A3[1000] = {0};


#include <mini-os/nnpback.h>
struct backend_param P264993A3_backend[16] = {(struct backend_param){.param_ptr = bias_1_264993A3, .param_size = 64}, (struct backend_param){.param_ptr = kernelData_1_264993A3, .param_size = 23232}, (struct backend_param){.param_ptr = bias_2_264993A3, .param_size = 192}, (struct backend_param){.param_ptr = kernelData_2_264993A3, .param_size = 307200}, (struct backend_param){.param_ptr = bias_3_264993A3, .param_size = 384}, (struct backend_param){.param_ptr = kernelData_3_264993A3, .param_size = 663552}, (struct backend_param){.param_ptr = bias_4_264993A3, .param_size = 256}, (struct backend_param){.param_ptr = kernelData_4_264993A3, .param_size = 884736}, (struct backend_param){.param_ptr = bias_5_264993A3, .param_size = 256}, (struct backend_param){.param_ptr = kernelData_5_264993A3, .param_size = 589824}, (struct backend_param){.param_ptr = kernelData_6_264993A3, .param_size = 37748736}, (struct backend_param){.param_ptr = bias_6_264993A3, .param_size = 4096}, (struct backend_param){.param_ptr = kernelData_7_264993A3, .param_size = 16777216}, (struct backend_param){.param_ptr = bias_7_264993A3, .param_size = 4096}, (struct backend_param){.param_ptr = kernelData_8_264993A3, .param_size = 4096000}, (struct backend_param){.param_ptr = bias_8_264993A3, .param_size = 1000}};