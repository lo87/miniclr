////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifndef _SPOT_NATIVE_H_
#define _SPOT_NATIVE_H_

#include <TinyCLR_Interop.h>

#if defined(PLATFORM_WINDOWS) 
#pragma comment(lib,"crypto")
#endif

#include <crypto.h>

typedef BOOL (*CryptoFpn)( BYTE *Key, BYTE *IV, DWORD cbIVSize, BYTE* pPlainText, DWORD cbPlainText, BYTE *pCypherText, DWORD cbCypherText );

struct Library_spot_native_Microsoft_SPOT_CryptoState
{
    struct Buffer
    {
        CLR_RT_HeapBlock_Array* m_array;
        int                     m_offset;
        int                     m_count;

        HRESULT Parse   ( CLR_RT_HeapBlock* ptr           );
        HRESULT Allocate( CLR_RT_HeapBlock& ref, int size );

        CLR_UINT8* Data();
    };

    Buffer                  m_dataIn;
    Buffer                  m_dataOut;
    CLR_RT_HeapBlock_Array* m_IV;
    KeySeed                 m_keySymmetric;
    RSAKey                  m_keyAsymmetric;
    void*                   m_handle;
    CRYPTO_RESULT           m_res;

    //--//
   
    void Init();
    void Cleanup();

    HRESULT LoadSymmetricKey ( CLR_RT_StackFrame& stack );
    HRESULT LoadAsymmetricKey( CLR_RT_StackFrame& stack );
    
    BOOL          Execute( CryptoFpn fpn );
    CRYPTO_RESULT StepRSA(               );

    static void GetSeed     ( KeySeed& seed );
    static void GetDeviceKey( UINT8*   key  );
    static void GetDeviceKey( RSAKey&  key  );


};

struct Library_spot_native_Microsoft_SPOT_Reflection
{
    TINYCLR_NATIVE_DECLARE(GetTypesImplementingInterface___STATIC__SZARRAY_mscorlibSystemType__mscorlibSystemType);
    TINYCLR_NATIVE_DECLARE(IsTypeLoaded___STATIC__BOOLEAN__mscorlibSystemType);
    TINYCLR_NATIVE_DECLARE(GetTypeHash___STATIC__U4__mscorlibSystemType);
    TINYCLR_NATIVE_DECLARE(GetAssemblyHash___STATIC__U4__mscorlibSystemReflectionAssembly);
    TINYCLR_NATIVE_DECLARE(GetAssemblies___STATIC__SZARRAY_mscorlibSystemReflectionAssembly);
    TINYCLR_NATIVE_DECLARE(GetAssemblyInfo___STATIC__BOOLEAN__SZARRAY_U1__MicrosoftSPOTReflectionAssemblyInfo);
    TINYCLR_NATIVE_DECLARE(GetTypeFromHash___STATIC__mscorlibSystemType__U4);
    TINYCLR_NATIVE_DECLARE(GetAssemblyFromHash___STATIC__mscorlibSystemReflectionAssembly__U4);
    TINYCLR_NATIVE_DECLARE(Serialize___STATIC__SZARRAY_U1__OBJECT__mscorlibSystemType);
    TINYCLR_NATIVE_DECLARE(Deserialize___STATIC__OBJECT__SZARRAY_U1__mscorlibSystemType);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_Reflection__AssemblyInfo
{
    static const int FIELD__m_name  = 1;
    static const int FIELD__m_flags = 2;
    static const int FIELD__m_size  = 3;
    static const int FIELD__m_hash  = 4;
    static const int FIELD__m_refs  = 5;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_BaseEvent
{
    static const int FIELD__Source       = 1;
    static const int FIELD__EventMessage = 2;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Cryptography_Key_RSA
{
    static const int FIELD__m_modulus  = 1;
    static const int FIELD__m_exponent = 2;

    TINYCLR_NATIVE_DECLARE(Encrypt___SZARRAY_U1__SZARRAY_U1__I4__I4__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(Decrypt___SZARRAY_U1__SZARRAY_U1__I4__I4__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(VerifySignature___BOOLEAN__SZARRAY_U1__I4__I4__SZARRAY_U1__I4__I4);

    //--//

    static HRESULT AdvanceState( CLR_RT_StackFrame& stack, RSAOperations op );


};

struct Library_spot_native_Microsoft_SPOT_Cryptography_Key_TinyEncryptionAlgorithm
{
    static const int FIELD__m_value           = 1;
    static const int FIELD__m_doublyEncrypted = 2;

    TINYCLR_NATIVE_DECLARE(Encrypt___SZARRAY_U1__SZARRAY_U1__I4__I4__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(Decrypt___SZARRAY_U1__SZARRAY_U1__I4__I4__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(get_Signature___SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(GetActivationString___STRING__U2__U2);

    //--//

    static HRESULT Initialize( CLR_RT_StackFrame& stack, Library_spot_native_Microsoft_SPOT_CryptoState& st, bool fKeyOnly, int resultLength );
};

struct Library_spot_native_Microsoft_SPOT_Debug
{
    TINYCLR_NATIVE_DECLARE(Print___STATIC__VOID__STRING);
    TINYCLR_NATIVE_DECLARE(GC___STATIC__U4__BOOLEAN);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_EventArgs
{
    static const int FIELD_STATIC__Empty = 0;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_ExecutionConstraint
{
    TINYCLR_NATIVE_DECLARE(Install___STATIC__VOID__I4__I4);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_ExtendedTimer
{
    static const int FIELD__m_timer    = 1;
    static const int FIELD__m_state    = 2;
    static const int FIELD__m_callback = 3;

    TINYCLR_NATIVE_DECLARE(Dispose___VOID);
    TINYCLR_NATIVE_DECLARE(_ctor___VOID__mscorlibSystemThreadingTimerCallback__OBJECT__I4__I4);
    TINYCLR_NATIVE_DECLARE(_ctor___VOID__mscorlibSystemThreadingTimerCallback__OBJECT__mscorlibSystemTimeSpan__mscorlibSystemTimeSpan);
    TINYCLR_NATIVE_DECLARE(_ctor___VOID__mscorlibSystemThreadingTimerCallback__OBJECT__mscorlibSystemDateTime__mscorlibSystemTimeSpan);
    TINYCLR_NATIVE_DECLARE(_ctor___VOID__mscorlibSystemThreadingTimerCallback__OBJECT__MicrosoftSPOTExtendedTimerTimeEvents);
    TINYCLR_NATIVE_DECLARE(Change___VOID__I4__I4);
    TINYCLR_NATIVE_DECLARE(Change___VOID__mscorlibSystemTimeSpan__mscorlibSystemTimeSpan);
    TINYCLR_NATIVE_DECLARE(Change___VOID__mscorlibSystemDateTime__mscorlibSystemTimeSpan);
    TINYCLR_NATIVE_DECLARE(get_LastExpiration___mscorlibSystemTimeSpan);

    //--//
  
    static const int c_TimeEvents_Second   = 0;
    static const int c_TimeEvents_Minute   = 1;
    static const int c_TimeEvents_Hour     = 2;
    static const int c_TimeEvents_Day      = 3;
    static const int c_TimeEvents_TimeZone = 4;
    static const int c_TimeEvents_SetTime  = 5;

    static HRESULT SetValues( CLR_RT_StackFrame& stack, CLR_UINT32 flags );

};

struct Library_spot_native_Microsoft_SPOT_ExtendedWeakReference
{
    TINYCLR_NATIVE_DECLARE(_ctor___VOID__OBJECT__mscorlibSystemType__U4__U4);
    TINYCLR_NATIVE_DECLARE(get_Selector___mscorlibSystemType);
    TINYCLR_NATIVE_DECLARE(get_Id___U4);
    TINYCLR_NATIVE_DECLARE(get_Flags___U4);
    TINYCLR_NATIVE_DECLARE(get_Priority___I4);
    TINYCLR_NATIVE_DECLARE(set_Priority___VOID__I4);
    TINYCLR_NATIVE_DECLARE(PushBackIntoRecoverList___VOID);
    TINYCLR_NATIVE_DECLARE(Recover___STATIC__MicrosoftSPOTExtendedWeakReference__mscorlibSystemType__U4);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_GenericEvent
{
    static const int FIELD__EventCategory = 3;
    static const int FIELD__EventData     = 4;
    static const int FIELD__X             = 5;
    static const int FIELD__Y             = 6;
    static const int FIELD__Time          = 7;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Hardware_SystemInfo
{
    TINYCLR_NATIVE_DECLARE(GetSystemVersion___STATIC__VOID__BYREF_I4__BYREF_I4__BYREF_I4__BYREF_I4);
    TINYCLR_NATIVE_DECLARE(get_OEMString___STATIC__STRING);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_Hardware_Utility
{
    TINYCLR_NATIVE_DECLARE(ComputeCRC___STATIC__U4__SZARRAY_U1__I4__I4__U4);
    TINYCLR_NATIVE_DECLARE(ExtractValueFromArray___STATIC__U4__SZARRAY_U1__I4__I4);
    TINYCLR_NATIVE_DECLARE(InsertValueIntoArray___STATIC__VOID__SZARRAY_U1__I4__I4__U4);
    TINYCLR_NATIVE_DECLARE(ExtractRangeFromArray___STATIC__SZARRAY_U1__SZARRAY_U1__I4__I4);
    TINYCLR_NATIVE_DECLARE(CombineArrays___STATIC__SZARRAY_U1__SZARRAY_U1__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(CombineArrays___STATIC__SZARRAY_U1__SZARRAY_U1__I4__I4__SZARRAY_U1__I4__I4);
    TINYCLR_NATIVE_DECLARE(SetLocalTime___STATIC__VOID__mscorlibSystemDateTime);
    TINYCLR_NATIVE_DECLARE(GetMachineTime___STATIC__mscorlibSystemTimeSpan);
    TINYCLR_NATIVE_DECLARE(Piezo___STATIC__VOID__U4__U4);
    TINYCLR_NATIVE_DECLARE(Backlight___STATIC__VOID__BOOLEAN);

    //--//
    
    static HRESULT InsertOrExtractValueFromArray( CLR_RT_StackFrame& stack, bool fInsert );

    static HRESULT CombineArrays( CLR_RT_StackFrame& stack, CLR_RT_HeapBlock& ref1, int offset1, int count1, CLR_RT_HeapBlock& ref2, int offset2, int count2 );

};

struct Library_spot_native_Microsoft_SPOT_Math
{
    TINYCLR_NATIVE_DECLARE(Cos___STATIC__I4__I4);
    TINYCLR_NATIVE_DECLARE(Sin___STATIC__I4__I4);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_Messaging_EndPoint
{
    static const int FIELD__m_handle = 1;

    TINYCLR_NATIVE_DECLARE(_ctor___VOID__mscorlibSystemType__U4);
    TINYCLR_NATIVE_DECLARE(Check___BOOLEAN__mscorlibSystemType__U4__I4);
    TINYCLR_NATIVE_DECLARE(GetMessage___MicrosoftSPOTMessagingMessage__I4);
    TINYCLR_NATIVE_DECLARE(SendMessageRaw___SZARRAY_U1__mscorlibSystemType__U4__I4__SZARRAY_U1);
    TINYCLR_NATIVE_DECLARE(ReplyRaw___VOID__MicrosoftSPOTMessagingMessage__SZARRAY_U1);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_Messaging_Message
{
    static const int FIELD__m_source   = 1;
    static const int FIELD__m_selector = 2;
    static const int FIELD__m_id       = 3;
    static const int FIELD__m_seq      = 4;
    static const int FIELD__m_payload  = 5;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Native_Resources
{
    static const int FIELD_STATIC__manager = 1;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_ResourceUtility
{
    static const int FIELD_STATIC__s_ewr = 2;

    TINYCLR_NATIVE_DECLARE(GetObject___STATIC__OBJECT__mscorlibSystemResourcesResourceManager__mscorlibSystemEnum);
    TINYCLR_NATIVE_DECLARE(set_CurrentUICultureInternal___STATIC__VOID__mscorlibSystemGlobalizationCultureInfo);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_SerializationHintsAttribute
{
    static const int FIELD__Flags     = 1;
    static const int FIELD__ArraySize = 2;
    static const int FIELD__BitPacked = 3;
    static const int FIELD__RangeBias = 4;
    static const int FIELD__Scale     = 5;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_SystemTime
{
    static const int FIELD__Year         = 1;
    static const int FIELD__Month        = 2;
    static const int FIELD__DayOfWeek    = 3;
    static const int FIELD__Day          = 4;
    static const int FIELD__Hour         = 5;
    static const int FIELD__Minute       = 6;
    static const int FIELD__Second       = 7;
    static const int FIELD__Milliseconds = 8;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_TimeZoneInformation
{
    static const int FIELD__Bias         = 1;
    static const int FIELD__StandardName = 2;
    static const int FIELD__StandardDate = 3;
    static const int FIELD__StandardBias = 4;
    static const int FIELD__DaylightName = 5;
    static const int FIELD__DaylightDate = 6;
    static const int FIELD__DaylightBias = 7;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Touch_TouchEvent
{
    static const int FIELD__Time    = 3;
    static const int FIELD__Touches = 4;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Touch_TouchInput
{
    static const int FIELD__X             = 1;
    static const int FIELD__Y             = 2;
    static const int FIELD__SourceID      = 3;
    static const int FIELD__Flags         = 4;
    static const int FIELD__ContactWidth  = 5;
    static const int FIELD__ContactHeight = 6;

    static const CLR_UINT32 FLAG__None    = 0x00;
    static const CLR_UINT32 FLAG__Primary = 0x10;
    static const CLR_UINT32 FLAG__Pen     = 0x40;
    static const CLR_UINT32 FLAG__Palm    = 0x80;

    //--//

};

struct Library_spot_native_Microsoft_SPOT_UnknownTypeException
{
    static const int FIELD__m_type = 5;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_WeakDelegate
{
    TINYCLR_NATIVE_DECLARE(Combine___STATIC__mscorlibSystemDelegate__mscorlibSystemDelegate__mscorlibSystemDelegate);
    TINYCLR_NATIVE_DECLARE(Remove___STATIC__mscorlibSystemDelegate__mscorlibSystemDelegate__mscorlibSystemDelegate);

    //--//

};

struct Library_spot_native_Microsoft_SPOT_Messaging_Message__RemotedException
{
    static const int FIELD__m_message = 1;


    //--//

};

struct Library_spot_native_Microsoft_SPOT_Hardware_SystemInfo__SystemID
{
    TINYCLR_NATIVE_DECLARE(get_OEM___STATIC__U1);
    TINYCLR_NATIVE_DECLARE(get_Model___STATIC__U1);
    TINYCLR_NATIVE_DECLARE(get_SKU___STATIC__U2);

    //--//

};



extern const CLR_RT_NativeAssemblyData g_CLR_AssemblyNative_Microsoft_SPOT_Native;

#endif  //_SPOT_NATIVE_H_
