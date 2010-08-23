////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CorLib.h"

HRESULT Library_corlib_native_System_Number::FormatNative___STATIC__STRING__OBJECT__CHAR__I4( CLR_RT_StackFrame& stack )
{
    NATIVE_PROFILE_CLR_CORE();
    TINYCLR_HEADER();

    char result[ 256 ];

    CLR_RT_HeapBlock* pArgs = &(stack.Arg0());
    CLR_RT_HeapBlock* value = pArgs;
    
    CLR_DataType dt;

    CLR_RT_TypeDescriptor desc;
    TINYCLR_CHECK_HRESULT(desc.InitializeFromObject( *value ));
    TINYCLR_CHECK_HRESULT(value->PerformUnboxing( desc.m_handlerCls ));
    

    dt = value->DataType();

    switch(dt)
    {
    case DATATYPE_I4: 
        hal_snprintf( result, ARRAYSIZE(result), "%d", value->NumericByRef().s4 ); 
        break;
    case DATATYPE_U4: 
        hal_snprintf( result, ARRAYSIZE(result), "%u", value->NumericByRef().u4 ); 
        break;
#if defined(PLATFORM_WINDOWS) || defined(PLATFORM_WINCE)
    case DATATYPE_I8: 
        hal_snprintf( result, ARRAYSIZE(result), "%I64d",(CLR_INT64_TEMP_CAST)value->NumericByRef().s8 ); 
        break;
    case DATATYPE_U8:
        hal_snprintf( result, ARRAYSIZE(result), "%I64u",(CLR_UINT64_TEMP_CAST)value->NumericByRef().u8 ); 
        break;
#else
    case DATATYPE_I8: 
        {   // use local 64 bit variable to assure 8-byte boundary.
            INT64 i64 = (CLR_INT64_TEMP_CAST)value->NumericByRef().s8;
            hal_snprintf( result, ARRAYSIZE(result), "%lld", i64); 
        }
        break;
    case DATATYPE_U8: 
        {   // use local 64 bit variable to assure 8-byte boundary.
            UINT64 ui64 = (CLR_UINT64_TEMP_CAST)value->NumericByRef().u8;
            hal_snprintf( result, ARRAYSIZE(result), "%llu", ui64 ); 
        }
        break;
#endif
    case DATATYPE_R4:
    case DATATYPE_R8:
        {
            // This is temporary buffer to create proper format string.  
            char formatStr[ 8 ];
            char formatCh = (char)pArgs[ 1 ].NumericByRef().u1;

            // All the formatCh have been converted to upper case in the managed layer
            _ASSERTE(formatCh == 'G' || formatCh == 'N' || formatCh == 'F');

            int precision = pArgs[ 2 ].NumericByRef().s4;
            
            if (precision < 0 || precision > 99) 
            {
                TINYCLR_SET_AND_LEAVE( CLR_E_INVALID_PARAMETER );
            }

            if (formatCh == 'G') 
            {
                hal_snprintf( formatStr, ARRAYSIZE(formatStr), "%%.%dg", (dt == DATATYPE_R4) ? 9 : 17 ); // "%.9g" for float, "%.17g" for double
            }
            else
            {
                hal_snprintf( formatStr, ARRAYSIZE(formatStr), "%%.%df", precision ); // i.e. "%#.2f" when precision is 2
            }

            if(dt == DATATYPE_R4)
            {
#if defined(_WIN32) || defined(WIN32) || defined(_WIN32_WCE)
                hal_snprintf( result, ARRAYSIZE(result), formatStr, value->NumericByRef().r4 );
#else
                float f = value->NumericByRef().r4;
                hal_snprintf_float( result, ARRAYSIZE(result), formatStr, f); 
#endif
            }
            else
            {
#if defined(_WIN32) || defined(WIN32) || defined(_WIN32_WCE)
                hal_snprintf( result, ARRAYSIZE(result), formatStr, (CLR_DOUBLE_TEMP_CAST)value->NumericByRef().r8 );
#else
                double d = (CLR_DOUBLE_TEMP_CAST)value->NumericByRef().r8;
                hal_snprintf_double( result, ARRAYSIZE(result), formatStr, d ); 
#endif                
            }
        }
        break;
    default:
        TINYCLR_SET_AND_LEAVE(CLR_E_FAIL);
    }

    TINYCLR_SET_AND_LEAVE(stack.SetResult_String( result ));

    TINYCLR_NOCLEANUP();
}   
