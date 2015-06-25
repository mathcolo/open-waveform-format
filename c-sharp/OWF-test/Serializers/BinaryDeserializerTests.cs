﻿using System;
using System.Web;
using Microsoft.VisualStudio.TestTools.UnitTesting;

using OWF.Serializers;
using OWF.DTO;
using System.Collections.Generic;

namespace OWF_test.Serializers
{
    [TestClass]
    public class BinaryDeserializerTests
    {
        private byte[] ReadOWF(string filename)
        {
            return System.IO.File.ReadAllBytes(String.Join("/", "..", "..", "..", "..", "example", "owf1_" + filename + ".owf"));
        }

        [TestMethod]
        public void DeserializesValidEmptyPacket()
        {
            byte[] buf = ReadOWF("binary_valid_empty");
            Package p = BinaryDeserializer.convert(buf);
            Assert.AreEqual(p.Channels.Count, 0);
        }

        [TestMethod]
        public void DeserializesValidEmptyChannel()
        {
            byte[] buf = ReadOWF("binary_valid_empty_channel");
            Package p = BinaryDeserializer.convert(buf);
            Assert.AreEqual(p.Channels.Count, 1);
            Assert.AreEqual(p.Channels[0].Id, "BED_42");
            Assert.AreEqual(p.Channels[0].Namespaces.Count, 0);
        }

        [TestMethod]
        public void DeserializesValidEmptyNamespace()
        {
            byte[] buf = ReadOWF("binary_valid_empty_namespace");
            Package p = BinaryDeserializer.convert(buf);
            Assert.AreEqual(p.Channels.Count, 1);
            Assert.AreEqual(p.Channels[0].Id, "BED_42");
            Assert.AreEqual(p.Channels[0].Namespaces.Count, 1);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Id, "GEWAVE");
            Assert.AreEqual(p.Channels[0].Namespaces[0].dt, new TimeSpan(0, 0, 3));
            Assert.AreEqual(p.Channels[0].Namespaces[0].Signals.Count, 0);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Events.Count, 0);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms.Count, 0);
        }

        [TestMethod]
        public void DeserializesValidPacket1()
        {
            byte[] buf = ReadOWF("binary_valid_1");
            Package p = BinaryDeserializer.convert(buf);
            Assert.AreEqual(p.Channels.Count, 1);
            Assert.AreEqual(p.Channels[0].Id, "BED_42");
            Assert.AreEqual(p.Channels[0].Namespaces.Count, 1);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Id, "GEWAVE");
            Assert.AreEqual(p.Channels[0].Namespaces[0].dt, new TimeSpan(0, 0, 3));
            Assert.AreEqual(p.Channels[0].Namespaces[0].Signals.Count, 1);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Signals[0].Id, "ECG_LEAD_2");
            Assert.AreEqual(p.Channels[0].Namespaces[0].Signals[0].Unit, "mV");
            Assert.AreEqual(p.Channels[0].Namespaces[0].Signals[0].Samples.Length, 11);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Events[0].Data, "POST OK");
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms.Count, 1);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms[0].Level, 0);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms[0].Volume, 255);
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms[0].Type, "SPO2 LO");
            Assert.AreEqual(p.Channels[0].Namespaces[0].Alarms[0].Message, "43");
        }
    }
}
