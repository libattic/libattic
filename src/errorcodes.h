

#ifndef ERROR_CODES_H_
#define ERROR_CODES_H_
#pragma once

namespace ret
{
    enum eCode
    {
        A_OK = 0,
        A_FAIL,
        A_FAIL_OPEN,
        A_FAIL_ENCRYPT,
        A_FAIL_DECRYPT,
        A_FAIL_COMPRESS,
        A_FAIL_DECOMPRESS,
        A_FAIL_INVALID_PTR,
        A_FAIL_VERIFY_CHUNKS,
        A_FAIL_WRITE_CHUNK

    };

};



#endif
