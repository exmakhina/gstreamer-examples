#!/usr/bin/env python
# -*- coding: utf-8 vi:noet
# Merger element for gstreamer

"""
Caps are not negotiated... yet.

"""

import collections

import numpy as np

import gi

gi.require_version('Gst', '1.0')
gi.require_version('GstBase', '1.0')

from gi.repository import Gst, GObject, GstBase

Gst.init(None)

class Merger(Gst.Element):
	__gstmetadata__ = (
	 "Merge element demo",
	 "blah",
	 "blah blah",
	 "Jérôme Carretero <cJ@zougloub.eu>"
	)

	def __init__(self):

		srcv_template = Gst.PadTemplate.new (
		 'src_v',
		 Gst.PadDirection.SRC,
		 Gst.PadPresence.ALWAYS,
		 Gst.caps_from_string('video/x-raw'),
		)

		sinkl_template = Gst.PadTemplate.new (
		 'sink_l',
		 Gst.PadDirection.SINK,
		 Gst.PadPresence.ALWAYS,
		 Gst.caps_from_string('video/x-raw'),
		)

		sinkr_template = Gst.PadTemplate.new (
		 'sink_r',
		 Gst.PadDirection.SINK,
		 Gst.PadPresence.ALWAYS,
		 Gst.caps_from_string('video/x-raw'),
		)

		Gst.Element.__init__(self)

		self.sinklpad = Gst.Pad.new_from_template(sinkl_template, 'sink_l')
		self.sinklpad.set_chain_function_full(self._sinkl_chain, None)
		self.sinklpad.set_event_function_full(self._sinkl_event, None)
		self.add_pad(self.sinklpad)

		self.sinkrpad = Gst.Pad.new_from_template(sinkr_template, 'sink_r')
		self.sinkrpad.set_chain_function_full(self._sinkr_chain, None)
		self.sinkrpad.set_event_function_full(self._sinkr_event, None)
		self.add_pad(self.sinkrpad)

		self.srcvpad = Gst.Pad.new_from_template(srcv_template, 'src_v')
		self.srcvpad.set_event_function_full(self._srcv_event, None)
		self.add_pad(self.srcvpad)

		self._bufs = collections.deque()

	def _sinkl_chain(self, pad, parent, buf_l):
		"""
		Chain function
		"""
		Gst.info("L Buffer L %s ts=%s sz=%s" \
		 % (buf_l, Gst.TIME_ARGS(buf_l.pts), buf_l.get_size()))

		#print(dir(buf_l))
		#bcaps = buf_l.getcaps()
		#Gst.info("Buffer L caps %s" % bcaps)

		#caps = pad.query_caps()
		#print(caps)

		#print(dir(pad))

		while True:
			try:
				buf_r = self._bufs.popleft()
			except IndexError:
				break
			Gst.info("Buffer R %s ts=%s sz=%s" \
			 % (buf_r, Gst.TIME_ARGS(buf_r.pts), buf_r.get_size()))
			if buf_r.pts > buf_l.pts:
				Gst.info("Buffer R found")
				break
			else:
				pass
				#buf_r.unref()
		else:
			Gst.info("Buffer R not found")
			return Gst.FlowReturn.OK

		return Gst.FlowReturn.OK

		mf = Gst.MapFlags.READ#((1<<0))
		res, mil = buf_l.map(mf)
		#Gst.info("Buffer %s" % dir(mil))

		res, mir = buf_r.map(mf)

		# TODO Here, it would be nice to create an output buffer.

		"""
		src0_d = mil.data
		src1_d = mir.data

		buf_out = Gst.Buffer.new_allocate(None, 1280*480*4)
		v = (np.fromstring(src0_d, dtype=np.uint8) + np.fromstring(src1_d, dtype=np.uint8)) / 2
		#buf_out.fill(0, v.data)
		buf_out.dts = buf_l.dts
		buf_out.pts = buf_l.pts
		buf_out.duration = buf_l.duration

		"""

		"""
		res, mio = buf_out.map(mf)
		dst_d = mio.data
		dst_d[:] = b" "
		buf_out.unmap(mio)
		"""

		#res = self.srcvpad.push(buf_out)

		buf_r.unmap(mir)
		buf_l.unmap(mil)

		#buf_l.unref()
		#buf_r.unref()

		res = Gst.FlowReturn.OK
		return res

	def _sinkr_chain(self, pad, parent, buf_r):
		"""
		Chain function
		"""
		#Gst.info("R Buffer R %s ts=%s sz=%s" \
		# % (buf_r, Gst.TIME_ARGS(buf_r.pts), buf_r.get_size()))

		self._bufs.append(buf_r)

		#print(dir(buf))
		#buf.unref()
		res = Gst.FlowReturn.OK
		#res = Gst.FlowReturn.NOT_LINKED
		return res

	def _srcv_event(self, pad, parent, event):
		Gst.info("event %s" % event)
		return self.sinklpad.push_event(event) and self.sinkrpad.push_event(event)

	def _sinkl_event(self, pad, parent, event):
		Gst.info("event %s" % event)
		return self.srcvpad.push_event(event)

	def _sinkr_event(self, pad, parent, event):
		Gst.info("event %s" % event)
		return self.srcvpad.push_event(event)


GObject.type_register(Merger)

__gstelementfactory__ = ("merger", Gst.Rank.NONE, Merger)
