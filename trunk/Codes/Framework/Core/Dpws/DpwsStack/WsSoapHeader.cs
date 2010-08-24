using System;
using System.Collections;
using System.Text;
using System.Xml;
using Ws.Services.Faults;
using Microsoft.SPOT;
using Ws.Services.Xml;

namespace Ws.Services.WsaAddressing
{

    /// <summary>
    /// Parses, validates and stores the soap header information.
    /// </summary>
    /// <remarks>
    /// Use this class to acces the soap header properties and parameters.
    /// </remarks>
    public class WsWsaHeader
    {
        // Addressing header properties
        private String _action;
        private String _relatesTo;
        private String _to;
        private String _messageId;
        private WsWsaEndpointRef _replyTo;
        private WsWsaEndpointRef _from;
        private WsWsaEndpointRef _faultTo;
        private WsXmlNodeList _any;

        /// <summary>
        /// Use to Get or Set a soap header Action property.
        /// </summary>
        public string Action { get { return _action; } }

        /// <summary>
        /// Use to Get or Set a soap header RelatesTo property.
        /// </summary>
        public string RelatesTo { get { return _relatesTo; } }

        /// <summary>
        /// Use to Get or Set a soap header To property.
        /// </summary>
        public string To { get { return _to; } }

        /// <summary>
        /// Use to Get or Set a soap header MessageID property.
        /// </summary>
        public string MessageID { get { return _messageId; } }

        /// <summary>
        /// Use to Get or Set a soap header ReplyTo property.
        /// </summary>
        public WsWsaEndpointRef ReplyTo { get { return _replyTo; } }

        /// <summary>
        /// Use to Get or Set a soap header From property.
        /// </summary>
        public WsWsaEndpointRef From { get { return _from; } }

        /// <summary>
        /// Use to Get or Set a soap header FaultTo property.
        /// </summary>
        public WsWsaEndpointRef FaultTo { get { return _faultTo; } }

        public WsXmlNodeList Any { get { return _any; } }

        /// <summary>
        /// Creates an instance of the soap header class.
        /// </summary>
        public WsWsaHeader()
        {
            _any = new WsXmlNodeList();
        }

        public WsWsaHeader(String action, String relatesTo, String to, String replyTo, String from, WsXmlNodeList any)
        {
            _action = action;
            _relatesTo = relatesTo;
            _to = to;
            if (replyTo != null)
            {
                _replyTo = new WsWsaEndpointRef(new System.Uri(replyTo));
            }

            if (from != null)
            {
                _from = new WsWsaEndpointRef(new System.Uri(from));
            }

            _any = any;
        }

        [Flags]
        private enum RequiredHeaderElement : byte
        {
            None = 0x0,
            To = 0x1,
            MessageID = 0x2,
            Action = 0x4,
            All = To | MessageID | Action
        }

        internal void ParseHeader(XmlReader reader)
        {
            RequiredHeaderElement headerElements = RequiredHeaderElement.None;

            try
            {
                reader.ReadStartElement("Header", WsWellKnownUri.SoapNamespaceUri);

                while (true)
                {
                    if (reader.IsStartElement())
                    {
                        if (
                            (reader.NamespaceURI == WsWellKnownUri.WsaNamespaceUri_2005_08) ||
                            (reader.NamespaceURI == WsWellKnownUri.WsaNamespaceUri_2004_08) 
                           )
                        {
                            switch (reader.LocalName)
                            {
                                case "MessageID":
                                    if ((_messageId = reader.ReadElementString()) == String.Empty)
                                    {
                                        throw new XmlException("MessageID");
                                    }

                                    headerElements |= RequiredHeaderElement.MessageID;
                                    break;
                                case "RelatesTo":
                                    if ((_relatesTo = reader.ReadElementString()) == String.Empty)
                                    {
                                        throw new XmlException("RelatesTo");
                                    }
                                    break;
                                case "To":
                                    if ((_to = reader.ReadElementString()) == String.Empty)
                                    {
                                        throw new XmlException("To");
                                    }

                                    // If this is a URI peal off the transport address
                                    if (To.IndexOf("http://") == 0)
                                    {
                                        int pathIndex = _to.Substring(7).IndexOf('/');
                                        if (pathIndex != -1)
                                            _to = _to.Substring(pathIndex + 8);
                                    }

                                    headerElements |= RequiredHeaderElement.To;
                                    break;
                                case "Action":
                                    if ((_action = reader.ReadElementString()) == String.Empty)
                                    {
                                        throw new XmlException("Action");
                                    }

                                    headerElements |= RequiredHeaderElement.Action;
                                    break;
                                case "From":
#if DEBUG
                                    int depth = reader.Depth;
#endif
                                    _from = new WsWsaEndpointRef(reader);
#if DEBUG
                                    Debug.Assert(XmlReaderHelper.HasReadCompleteNode(depth, reader));
#endif
                                    break;
                                case "ReplyTo":
#if DEBUG
                                    depth = reader.Depth;
#endif
                                    _replyTo = new WsWsaEndpointRef(reader);
#if DEBUG
                                    Debug.Assert(XmlReaderHelper.HasReadCompleteNode(depth, reader));
#endif
                                    if (ReplyTo.Address.AbsoluteUri != WsWellKnownUri.WsaAnonymousUri &&
                                        ReplyTo.Address.AbsoluteUri != WsWellKnownUri.WsaAnonymousRoleUri)
                                    {
                                        throw new WsFaultException(this, WsFaultType.WsaInvalidMessageInformationHeader, "R0031");
                                    }
                                    break;
                                case "FaultTo":
#if DEBUG
                                    depth = reader.Depth;
#endif
                                    _faultTo = new WsWsaEndpointRef(reader);
#if DEBUG
                                    Debug.Assert(XmlReaderHelper.HasReadCompleteNode(depth, reader));
#endif
                                    break;
                                default:
                                    reader.Skip(); //unknown item from WSA namespace, should we store it?
                                    break;
                            }
                        }
                        else
                        {
                            // unknown item, need to store it for future reference
#if DEBUG
                            int depth = reader.Depth;
#endif
                            _any.Add(new WsXmlNode(reader));
#if DEBUG
                            Debug.Assert(XmlReaderHelper.HasReadCompleteNode(depth, reader));
#endif
                        }
                    }
                    else
                    {
                        break;
                    }
                }

                reader.ReadEndElement(); // Header
            }
            catch (XmlException e)
            {
                throw new WsFaultException(this, WsFaultType.WsaInvalidMessageInformationHeader, e.ToString());
            }

            //if (headerElements != RequiredHeaderElement.All)
            //{
            //    throw new WsFaultException(this, WsFaultType.WsaMessageInformationHeaderRequired);
            //}
        }
    }
}


