#pragma once
#define ASHMEM_NAME_LEN   256
#define __ASHMEMIOC       0x77
#define ASHMEM_SET_SIZE   _IOW(__ASHMEMIOC, 3, size_t)
#define ASHMEM_GET_SIZE   _IO(__ASHMEMIOC, 4)
