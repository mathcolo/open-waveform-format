﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace OWF
{
    /// <summary>
    /// OWFChannel contains the devices broadcasting on a particular data source, like a patient bed.
    /// </summary>
    public class Channel
    {
        public Channel( string id, List<Namespace> namespaces)
        {
            this.id = id;
            this.namespaces = namespaces;
        }

        readonly protected string id;
        public string Id
        {
            get
            {
                return id;
            }
        }

        readonly protected List<Namespace> namespaces;
        public List<Namespace> Namespace
        {
            get
            {
                return namespaces;
            }
        }
    }
}
