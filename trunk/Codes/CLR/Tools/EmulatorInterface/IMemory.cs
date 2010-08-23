////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using System;

namespace Microsoft.SPOT.Emulator.Memory
{
    public interface IMemoryDriver
    {        
        void HeapLocation(out IntPtr BaseAddress, out uint SizeInBytes);
        void CustomHeapLocation(out IntPtr BaseAddress, out uint SizeInBytes);
    }
}