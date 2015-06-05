﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace OWF
{
    public class Signal
    {
        public Signal(string id, string unit, double[] samples)
        {
            this.id = id;
            this.unit = unit;
            this.samples = samples;
        }

        readonly protected string id;
        public string Id
        {
            get
            {
                return Id;
            }
        }

        readonly protected string unit;
        public string Unit
        {
            get
            {
                return unit;
            }
        }

        readonly protected double[] samples;
        public double[] Samples
        {
            get
            {
                return samples;
            }
        }
    }
}
