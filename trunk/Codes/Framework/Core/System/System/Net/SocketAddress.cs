////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) Microsoft Corporation.  All rights reserved.
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace System.Net
{

    using System;
    using System.Runtime.InteropServices;
    using System.Net.Sockets;
    using System.Text;
    using System.Globalization;

    public class SocketAddress
    {
        internal const int IPv4AddressSize = 16;

        internal byte[] m_Buffer;

        public AddressFamily Family
        {
            get
            {
                int family;
#if BIGENDIAN
                //little-endianness is assumed is through much of the sockets implementation
                throw new Exception();
                family = ((int)m_Buffer[0]<<8) | m_Buffer[1];
#else
                //Endianness
                family = m_Buffer[0] | ((int)m_Buffer[1] << 8);
#endif
                return (AddressFamily)family;
            }
        }

        internal SocketAddress(byte[] address)
        {
            m_Buffer = address;
        }

        public SocketAddress(AddressFamily family, int size)
        {
            Microsoft.SPOT.Debug.Assert(size > 2);

            m_Buffer = new byte[size]; //(size / IntPtr.Size + 2) * IntPtr.Size];//sizeof DWORD

#if BIGENDIAN
            m_Buffer[0] = unchecked((byte)((int)family>>8));
            m_Buffer[1] = unchecked((byte)((int)family   ));
#else
            m_Buffer[0] = unchecked((byte)((int)family));
            m_Buffer[1] = unchecked((byte)((int)family >> 8));
#endif
        }

        public int Size
        {
            get { return m_Buffer.Length; }
        }

        public byte this[int offset]
        {
            get { return m_Buffer[offset]; }
            set { m_Buffer[offset] = value; }
        }

    } // class SocketAddress
} // namespace System.Net


